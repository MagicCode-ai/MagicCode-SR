#import "ViewController.h"

#import <AVFoundation/AVFoundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/QuartzCore.h>
#import <UIKit/UIKit.h>
#import <math.h>
#import <stdlib.h>
#import <string.h>

#include "mc_enable.h"

static const int32_t kInputWidth = 1920;
static const int32_t kInputHeight = 1080;
static const NSTimeInterval kScaleApplyDelay = 0.08;

@interface ViewController () <AVCaptureVideoDataOutputSampleBufferDelegate>
@property(nonatomic, strong) UIImageView *imageView;
@property(nonatomic, strong) UILabel *statusLabel;
@property(nonatomic, strong) UISegmentedControl *modeControl;
@property(nonatomic, strong) UISlider *scaleSlider;
@property(nonatomic, strong) UILabel *scaleLabel;
@property(nonatomic, strong) AVCaptureSession *session;
@property(nonatomic, strong) dispatch_queue_t captureQueue;
@property(nonatomic, strong) id<MTLDevice> device;
@property(nonatomic) alg_mode_e selectedMode;
@property(nonatomic) float selectedScale;
@property(nonatomic, strong) id<MTLTexture> inputTexture;
@property(nonatomic, strong) NSMutableData *fullRgbaData;
@property(nonatomic, strong) NSMutableData *cropRgbaData;
@property(nonatomic, strong) NSMutableData *outputRgbaData;
@property(nonatomic) BOOL processingFrame;
@property(nonatomic) size_t fullFrameWidth;
@property(nonatomic) size_t fullFrameHeight;
@property(nonatomic) size_t inputTexWidth;
@property(nonatomic) size_t inputTexHeight;
@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = UIColor.blackColor;
    self.captureQueue = dispatch_queue_create("magic.magnifier.capture", DISPATCH_QUEUE_SERIAL);
    self.device = MTLCreateSystemDefaultDevice();
    self.selectedMode = HIGH_SPEED_MODE;
    self.selectedScale = 2.0f;
    self.fullFrameWidth = 0;
    self.fullFrameHeight = 0;
    self.inputTexWidth = 0;
    self.inputTexHeight = 0;
    [self buildUi];

    @try {
        [self ensureModelsInSandbox];
    } @catch (NSException *exception) {
        [self failAndStop:[NSString stringWithFormat:@"error: %@", exception.reason ?: @"model setup failed"]];
        return;
    }
    [self requestCameraAndStart];
}

- (void)dealloc {
    MC_Disable(NULL);
    [self.session stopRunning];
}

- (void)buildUi {
    self.imageView = [[UIImageView alloc] initWithFrame:self.view.bounds];
    self.imageView.contentMode = UIViewContentModeScaleAspectFit;
    self.imageView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    self.imageView.backgroundColor = UIColor.blackColor;
    [self.view addSubview:self.imageView];

    UIView *panel = [[UIView alloc] initWithFrame:CGRectZero];
    panel.backgroundColor = [[UIColor blackColor] colorWithAlphaComponent:0.6];
    panel.layer.cornerRadius = 10.0;
    panel.clipsToBounds = YES;
    panel.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:panel];

    UIStackView *stack = [[UIStackView alloc] initWithFrame:CGRectZero];
    stack.axis = UILayoutConstraintAxisVertical;
    stack.spacing = 10;
    stack.translatesAutoresizingMaskIntoConstraints = NO;
    [panel addSubview:stack];

    self.modeControl = [[UISegmentedControl alloc] initWithItems:@[@"highspeed", @"speed"]];
    self.modeControl.selectedSegmentIndex = 0;
    [self.modeControl addTarget:self action:@selector(modeChanged:) forControlEvents:UIControlEventValueChanged];
    [stack addArrangedSubview:self.modeControl];

    UIStackView *scaleRow = [[UIStackView alloc] initWithFrame:CGRectZero];
    scaleRow.axis = UILayoutConstraintAxisHorizontal;
    scaleRow.spacing = 8;
    scaleRow.alignment = UIStackViewAlignmentCenter;
    self.scaleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    self.scaleLabel.textColor = UIColor.whiteColor;
    self.scaleLabel.font = [UIFont systemFontOfSize:13 weight:UIFontWeightMedium];
    [scaleRow addArrangedSubview:self.scaleLabel];

    self.scaleSlider = [[UISlider alloc] initWithFrame:CGRectZero];
    self.scaleSlider.minimumValue = 1.0f;
    self.scaleSlider.maximumValue = 8.0f;
    self.scaleSlider.value = self.selectedScale;
    [self.scaleSlider addTarget:self action:@selector(scaleChanged:) forControlEvents:UIControlEventValueChanged];
    [scaleRow addArrangedSubview:self.scaleSlider];
    [stack addArrangedSubview:scaleRow];

    self.statusLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    self.statusLabel.textColor = UIColor.whiteColor;
    self.statusLabel.font = [UIFont monospacedDigitSystemFontOfSize:12 weight:UIFontWeightRegular];
    self.statusLabel.numberOfLines = 2;
    self.statusLabel.text = @"initializing";
    [stack addArrangedSubview:self.statusLabel];

    [NSLayoutConstraint activateConstraints:@[
        [panel.leadingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.leadingAnchor constant:12],
        [panel.trailingAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.trailingAnchor constant:-12],
        [panel.topAnchor constraintEqualToAnchor:self.view.safeAreaLayoutGuide.topAnchor constant:8],
        [stack.leadingAnchor constraintEqualToAnchor:panel.leadingAnchor constant:12],
        [stack.trailingAnchor constraintEqualToAnchor:panel.trailingAnchor constant:-12],
        [stack.topAnchor constraintEqualToAnchor:panel.topAnchor constant:12],
        [stack.bottomAnchor constraintEqualToAnchor:panel.bottomAnchor constant:-12],
    ]];
    [self updateScaleLabel];
}

- (void)modeChanged:(UISegmentedControl *)sender {
    self.selectedMode = sender.selectedSegmentIndex == 1 ? SPEED_MODE : HIGH_SPEED_MODE;
    [self restartEngine];
}

- (void)scaleChanged:(UISlider *)sender {
    self.selectedScale = MAX(1.0f, MIN(8.0f, sender.value));
    [self updateScaleLabel];
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(restartEngine) object:nil];
    [self performSelector:@selector(restartEngine) withObject:nil afterDelay:kScaleApplyDelay];
}

- (void)updateScaleLabel {
    self.scaleLabel.text = [NSString stringWithFormat:@"scale=x%@", [self formatScale:self.selectedScale]];
}

- (NSString *)formatScale:(float)value {
    NSString *text = [NSString stringWithFormat:@"%.2f", value];
    while ([text hasSuffix:@"0"]) text = [text substringToIndex:text.length - 1];
    if ([text hasSuffix:@"."]) text = [text substringToIndex:text.length - 1];
    return text;
}

- (void)requestCameraAndStart {
    [AVCaptureDevice requestAccessForMediaType:AVMediaTypeVideo completionHandler:^(BOOL granted) {
        dispatch_async(dispatch_get_main_queue(), ^{
            if (!granted) {
                [self failAndStop:@"error: camera permission denied"];
                return;
            }
            [self startCapture];
        });
    }];
}

- (void)startCapture {
    if (self.session && self.session.isRunning) return;
    self.session = [[AVCaptureSession alloc] init];
    self.session.sessionPreset = AVCaptureSessionPresetInputPriority;

    AVCaptureDeviceDiscoverySession *discovery =
        [AVCaptureDeviceDiscoverySession discoverySessionWithDeviceTypes:@[AVCaptureDeviceTypeBuiltInWideAngleCamera]
                                                               mediaType:AVMediaTypeVideo
                                                                position:AVCaptureDevicePositionBack];
    AVCaptureDevice *camera = discovery.devices.firstObject ?: [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
    if (!camera) {
        [self failAndStop:@"error: no camera device"];
        return;
    }

    NSError *error = nil;
    AVCaptureDeviceInput *input = [AVCaptureDeviceInput deviceInputWithDevice:camera error:&error];
    if (!input || error) {
        [self failAndStop:[NSString stringWithFormat:@"error: camera input failed %@", error.localizedDescription ?: @""]];
        return;
    }
    [self configure1080pFormat:camera];

    AVCaptureVideoDataOutput *output = [[AVCaptureVideoDataOutput alloc] init];
    output.videoSettings = @{(id)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA)};
    output.alwaysDiscardsLateVideoFrames = YES;
    [output setSampleBufferDelegate:self queue:self.captureQueue];

    if ([self.session canAddInput:input]) [self.session addInput:input];
    if ([self.session canAddOutput:output]) [self.session addOutput:output];
    AVCaptureConnection *videoConn = [output connectionWithMediaType:AVMediaTypeVideo];
    if (videoConn.isVideoOrientationSupported) {
        videoConn.videoOrientation = AVCaptureVideoOrientationPortrait;
    }
    [self.session startRunning];
    self.statusLabel.text = @"camera ready";
}

- (void)configure1080pFormat:(AVCaptureDevice *)camera {
    AVCaptureDeviceFormat *target = nil;
    for (AVCaptureDeviceFormat *fmt in camera.formats) {
        CMVideoDimensions d = CMVideoFormatDescriptionGetDimensions(fmt.formatDescription);
        if (d.width == kInputWidth && d.height == kInputHeight) {
            target = fmt;
            break;
        }
    }
    if (!target) return;
    NSError *err = nil;
    if ([camera lockForConfiguration:&err]) {
        camera.activeFormat = target;
        [camera unlockForConfiguration];
    }
}

- (void)captureOutput:(AVCaptureOutput *)output
didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
       fromConnection:(AVCaptureConnection *)connection {
    (void)output;
    (void)connection;
    if (self.processingFrame) return;
    self.processingFrame = YES;

    CVPixelBufferRef pb = CMSampleBufferGetImageBuffer(sampleBuffer);
    if (!pb) {
        self.processingFrame = NO;
        return;
    }
    CVPixelBufferLockBaseAddress(pb, kCVPixelBufferLock_ReadOnly);
    size_t width = CVPixelBufferGetWidth(pb);
    size_t height = CVPixelBufferGetHeight(pb);

    @try {
        [self ensureFullFrameBufferForWidth:width height:height];
        [self fillFullRgbaFromPixelBuffer:pb];

        // Magnifier: crop center (1/scale), then SR that crop back up.
        size_t cropW = [self evenSize:llround((double)width / (double)self.selectedScale) max:width];
        size_t cropH = [self evenSize:llround((double)height / (double)self.selectedScale) max:height];
        size_t cropX = (width - cropW) / 2;
        size_t cropY = (height - cropH) / 2;
        [self ensureInputTextureForWidth:cropW height:cropH];
        [self uploadCropToInputTextureFromFullX:cropX y:cropY width:cropW height:cropH];
        [self prepareMetalEnableModelEnvironment];

        void *outPtr = MC_Enable_3params((__bridge void *)self.inputTexture,
                                         self.selectedScale,
                                         self.selectedMode);
        if (outPtr == NULL) {
            @throw [NSException exceptionWithName:@"MCEnableError"
                                           reason:[NSString stringWithFormat:@"MC_Enable_3params failed scale=%.2f mode=%d",
                                                   self.selectedScale, (int)self.selectedMode]
                                         userInfo:nil];
        }

        id<MTLTexture> outTexture = (__bridge id<MTLTexture>)outPtr;
        size_t outW = outTexture.width;
        size_t outH = outTexture.height;
        size_t outputBytes = outW * outH * 4;
        if (!self.outputRgbaData || self.outputRgbaData.length != outputBytes) {
            self.outputRgbaData = [NSMutableData dataWithLength:outputBytes];
        }
        [outTexture getBytes:self.outputRgbaData.mutableBytes
                 bytesPerRow:outW * 4
                  fromRegion:MTLRegionMake2D(0, 0, outW, outH)
                 mipmapLevel:0];
        UIImage *img = [self imageFromRgbaData:self.outputRgbaData width:outW height:outH];
        NSString *status = [NSString stringWithFormat:@"mode=%@ scale=x%@ crop=%zux%zu out=%zux%zu",
                            self.selectedMode == SPEED_MODE ? @"speed" : @"highspeed",
                            [self formatScale:self.selectedScale],
                            cropW, cropH, outW, outH];
        dispatch_async(dispatch_get_main_queue(), ^{
            self.imageView.image = img;
            self.statusLabel.text = status;
        });
    } @catch (NSException *ex) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self failAndStop:[NSString stringWithFormat:@"error: %@", ex.reason ?: @"process failed"]];
        });
    }

    CVPixelBufferUnlockBaseAddress(pb, kCVPixelBufferLock_ReadOnly);
    self.processingFrame = NO;
}

- (void)prepareMetalEnableModelEnvironment {
    NSString *modelPath = [self modelPathForCurrentMode];
    setenv("MAGIC_SR_MODEL", modelPath.UTF8String, 1);
}

- (size_t)evenSize:(long long)desired max:(size_t)max {
    size_t v = (size_t)MAX(2LL, MIN(desired, (long long)max));
    return v & ~(size_t)1;
}

- (void)ensureFullFrameBufferForWidth:(size_t)width height:(size_t)height {
    size_t bytes = width * height * 4;
    if (self.fullRgbaData && self.fullFrameWidth == width && self.fullFrameHeight == height &&
        self.fullRgbaData.length == bytes) {
        return;
    }
    self.fullRgbaData = [NSMutableData dataWithLength:bytes];
    self.fullFrameWidth = width;
    self.fullFrameHeight = height;
}

- (void)ensureInputTextureForWidth:(size_t)width height:(size_t)height {
    if (self.inputTexture && self.inputTexWidth == width && self.inputTexHeight == height) {
        return;
    }
    MTLTextureDescriptor *inDesc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                      width:width
                                                                                     height:height
                                                                                  mipmapped:NO];
    inDesc.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
    self.inputTexture = [self.device newTextureWithDescriptor:inDesc];
    if (!self.inputTexture) {
        @throw [NSException exceptionWithName:@"TextureCreateError" reason:@"failed to create input texture" userInfo:nil];
    }
    self.inputTexWidth = width;
    self.inputTexHeight = height;
}

- (void)fillFullRgbaFromPixelBuffer:(CVPixelBufferRef)pb {
    uint8_t *src = (uint8_t *)CVPixelBufferGetBaseAddress(pb);
    size_t width = CVPixelBufferGetWidth(pb);
    size_t height = CVPixelBufferGetHeight(pb);
    size_t stride = CVPixelBufferGetBytesPerRow(pb);
    uint8_t *dst = (uint8_t *)self.fullRgbaData.mutableBytes;
    for (size_t y = 0; y < height; y++) {
        const uint8_t *row = src + y * stride;
        uint8_t *out = dst + y * width * 4;
        for (size_t x = 0; x < width; x++) {
            const uint8_t b = row[x * 4 + 0];
            const uint8_t g = row[x * 4 + 1];
            const uint8_t r = row[x * 4 + 2];
            const uint8_t a = row[x * 4 + 3];
            out[x * 4 + 0] = r;
            out[x * 4 + 1] = g;
            out[x * 4 + 2] = b;
            out[x * 4 + 3] = a;
        }
    }
}

- (void)uploadCropToInputTextureFromFullX:(size_t)cropX y:(size_t)cropY width:(size_t)cropW height:(size_t)cropH {
    const uint8_t *src = (const uint8_t *)self.fullRgbaData.bytes;
    size_t fullW = self.fullFrameWidth;
    size_t bytes = cropW * cropH * 4;
    if (!self.cropRgbaData || self.cropRgbaData.length != bytes) {
        self.cropRgbaData = [NSMutableData dataWithLength:bytes];
    }
    uint8_t *dst = (uint8_t *)self.cropRgbaData.mutableBytes;
    for (size_t y = 0; y < cropH; y++) {
        memcpy(dst + y * cropW * 4,
               src + ((cropY + y) * fullW + cropX) * 4,
               cropW * 4);
    }
    [self.inputTexture replaceRegion:MTLRegionMake2D(0, 0, cropW, cropH)
                         mipmapLevel:0
                           withBytes:self.cropRgbaData.bytes
                         bytesPerRow:cropW * 4];
}

- (UIImage *)imageFromRgbaData:(NSData *)rgba width:(size_t)width height:(size_t)height {
    if (!rgba || width == 0 || height == 0) return nil;
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    if (!cs) return nil;
    CGDataProviderRef provider = CGDataProviderCreateWithData(NULL, rgba.bytes, rgba.length, NULL);
    if (!provider) {
        CGColorSpaceRelease(cs);
        return nil;
    }
    CGImageRef imageRef = CGImageCreate(width,
                                        height,
                                        8,
                                        32,
                                        width * 4,
                                        cs,
                                        kCGBitmapByteOrderDefault | kCGImageAlphaLast,
                                        provider,
                                        NULL,
                                        false,
                                        kCGRenderingIntentDefault);
    UIImage *img = imageRef ? [UIImage imageWithCGImage:imageRef scale:1.0 orientation:UIImageOrientationUp] : nil;
    if (imageRef) CGImageRelease(imageRef);
    CGDataProviderRelease(provider);
    CGColorSpaceRelease(cs);
    return img;
}

- (void)ensureModelsInSandbox {
    NSArray<NSString *> *names = @[@"magic_metal_highspeed_gpu_params.bin", @"magic_metal_speed_gpu_params.bin"];
    NSString *docs = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject;
    NSFileManager *fm = [NSFileManager defaultManager];
    for (NSString *name in names) {
        NSString *dst = [docs stringByAppendingPathComponent:name];
        if ([fm fileExistsAtPath:dst]) continue;
        NSString *src = [NSBundle.mainBundle pathForResource:[name stringByDeletingPathExtension]
                                                      ofType:name.pathExtension];
        if (!src) {
            @throw [NSException exceptionWithName:@"ModelMissing" reason:[NSString stringWithFormat:@"missing bundle model: %@", name] userInfo:nil];
        }
        NSError *err = nil;
        [fm copyItemAtPath:src toPath:dst error:&err];
        if (err || ![fm fileExistsAtPath:dst]) {
            @throw [NSException exceptionWithName:@"ModelCopyFailed"
                                           reason:[NSString stringWithFormat:@"copy failed %@: %@", name, err.localizedDescription ?: @"unknown"]
                                         userInfo:nil];
        }
    }
}

- (NSString *)modelPathForCurrentMode {
    NSString *name = self.selectedMode == SPEED_MODE
        ? @"magic_metal_speed_gpu_params.bin"
        : @"magic_metal_highspeed_gpu_params.bin";
    NSString *path = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES).firstObject
                      stringByAppendingPathComponent:name];
    NSDictionary *attrs = [[NSFileManager defaultManager] attributesOfItemAtPath:path error:nil];
    if (!attrs || attrs.fileSize <= 0) {
        @throw [NSException exceptionWithName:@"ModelMissing" reason:[NSString stringWithFormat:@"model missing: %@", path] userInfo:nil];
    }
    return path;
}

- (void)restartEngine {
    dispatch_async(self.captureQueue, ^{
        [self restartEngineUnsafe];
    });
}

- (void)restartEngineUnsafe {
    MC_Disable(NULL);
    self.inputTexture = nil;
    self.fullRgbaData = nil;
    self.cropRgbaData = nil;
    self.outputRgbaData = nil;
    self.fullFrameWidth = 0;
    self.fullFrameHeight = 0;
    self.inputTexWidth = 0;
    self.inputTexHeight = 0;
}

- (void)failAndStop:(NSString *)message {
    [self.session stopRunning];
    dispatch_sync(self.captureQueue, ^{
        [self restartEngineUnsafe];
    });
    NSLog(@"[MagicMagnifierSR] failAndStop: %@", message ?: @"nil");
    self.statusLabel.text = message.length ? message : @"error";
}

@end
