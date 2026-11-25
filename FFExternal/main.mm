#import <UIKit/UIKit.h>
#import <AVFoundation/AVFoundation.h>
#import <QuartzCore/QuartzCore.h>
#include <thread>
#include <chrono>
#include "hack.hpp"

extern "C" void set_log_path(const char* path);

@interface OverlayView : UIView
@end

@implementation OverlayView
- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        self.backgroundColor = [UIColor clearColor];
        self.userInteractionEnabled = NO;
        self.opaque = NO;
    }
    return self;
}

- (void)drawRect:(CGRect)rect {
    CGContextRef ctx = UIGraphicsGetCurrentContext();
    if (!ctx) return;

    float cx = rect.size.width / 2;
    float cy = rect.size.height / 2;

    if (g_Config.drawFOV) {
        CGContextSetRGBStrokeColor(ctx, 1.0, 1.0, 1.0, 0.5);
        CGContextSetLineWidth(ctx, 1.5);
        CGContextAddArc(ctx, cx, cy, g_Config.aimFOV, 0, 2 * M_PI, 0);
        CGContextStrokePath(ctx);
    }

    if (g_Config.espEnabled) {
        std::lock_guard<std::mutex> lk(g_EspMutex);
        for (const auto& e : g_EspEntities) {
            float x = e.headPos.x;
            float y = rect.size.height - e.headPos.y;
            float w = 50.0f;
            float h = (rect.size.height - e.footPos.y) - y;

            if (h > 0) {
                if (g_Config.espBox) {
                    CGContextSetRGBStrokeColor(ctx, 1.0, 0.0, 0.0, 1.0);
                    CGContextSetLineWidth(ctx, 2.0);
                    CGContextStrokeRect(ctx, CGRectMake(x - (w/2), y, w, h));
                }
            }
        }
    }
}
@end

@interface ViewController : UIViewController
@property (nonatomic, strong) UILabel *debugLabel;
@end

@implementation ViewController
- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = [UIColor colorWithRed:0.05 green:0.05 blue:0.08 alpha:1.0];
    
    float y = 50.0f;
    
    UILabel *title = [[UILabel alloc] initWithFrame:CGRectMake(20, y, 300, 40)];
    title.text = @"FF EXTERNAL HUD";
    title.textColor = [UIColor greenColor];
    title.font = [UIFont boldSystemFontOfSize:24];
    [self.view addSubview:title];
    y += 60;

    self.debugLabel = [[UILabel alloc] initWithFrame:CGRectMake(20, y, 350, 80)];
    self.debugLabel.text = @"Status: Iniciando...";
    self.debugLabel.textColor = [UIColor yellowColor];
    self.debugLabel.numberOfLines = 3;
    self.debugLabel.font = [UIFont monospacedSystemFontOfSize:12 weight:UIFontWeightRegular];
    [self.view addSubview:self.debugLabel];
    y += 90;
    
    UILabel *logPathLabel = [[UILabel alloc] initWithFrame:CGRectMake(20, y, 350, 40)];
    logPathLabel.text = @"Log: Filza -> Documents/ff_log.txt";
    logPathLabel.textColor = [UIColor lightGrayColor];
    logPathLabel.font = [UIFont systemFontOfSize:12];
    [self.view addSubview:logPathLabel];

    [NSTimer scheduledTimerWithTimeInterval:0.5 repeats:YES block:^(NSTimer * _Nonnull timer) {
        dispatch_async(dispatch_get_main_queue(), ^{
            self.debugLabel.text = [NSString stringWithUTF8String:g_DebugInfo.c_str()];
        });
    }];
}
@end

@interface AppDelegate : UIResponder <UIApplicationDelegate>
@property (strong, nonatomic) UIWindow *window;
@property (strong, nonatomic) UIWindow *overlayWindow;
@property (strong, nonatomic) CADisplayLink *displayLink;
@end

@implementation AppDelegate
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    
    self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    self.window.rootViewController = [[ViewController alloc] init];
    [self.window makeKeyAndVisible];
    
    self.overlayWindow = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    self.overlayWindow.windowLevel = UIWindowLevelAlert + 1;
    self.overlayWindow.backgroundColor = [UIColor clearColor];
    self.overlayWindow.rootViewController = [[UIViewController alloc] init];
    self.overlayWindow.rootViewController.view.backgroundColor = [UIColor clearColor];
    
    OverlayView *overlay = [[OverlayView alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    [self.overlayWindow.rootViewController.view addSubview:overlay];
    self.overlayWindow.hidden = NO;
    self.overlayWindow.userInteractionEnabled = NO;

    self.displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(updateOverlay:)];
    [self.displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];

    AVAudioSession *session = [AVAudioSession sharedInstance];
    [session setCategory:AVAudioSessionCategoryPlayback withOptions:AVAudioSessionCategoryOptionMixWithOthers error:nil];
    [session setActive:YES error:nil];

    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
    NSString *documentsDirectory = [paths objectAtIndex:0];
    
    // AQUI ESTA A LINHA CORRIGIDA (Linha 141)
    NSString *logPath = [NSString stringWithFormat:@"%@/ff_log.txt", documentsDirectory];
    set_log_path([logPath UTF8String]);

    std::thread(HackLoop).detach();
    std::thread(AimbotThread).detach();
    std::thread(EspThread).detach();

    return YES;
}

- (void)updateOverlay:(CADisplayLink *)displayLink {
    [self.overlayWindow.rootViewController.view setNeedsDisplay];
}
@end

int main(int argc, char * argv[]) {
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
    }
}
