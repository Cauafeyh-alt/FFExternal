#import <UIKit/UIKit.h>
#import <AVFoundation/AVFoundation.h>
#include <thread>
#include <chrono>
#include "hack.hpp"

extern mach_port_t get_task_port(const char* process_name);
extern uintptr_t GetUnityBase();

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

            // ESP Box
            if (g_Config.espBox) {
                CGContextSetRGBStrokeColor(ctx, 1.0, 0.0, 0.0, 1.0);
                CGContextSetLineWidth(ctx, 2.0);
                CGContextStrokeRect(ctx, CGRectMake(x - (w/2), y, w, h));
            }

            // ESP Health
            if (g_Config.espHealth) {
                float hpWidth = 5.0f;
                float hpHeight = h;
                float hpPercentage = e.health / 200.0f; // Assumindo max HP 200
                if (hpPercentage > 1.0f) hpPercentage = 1.0f;
                
                CGContextSetRGBFillColor(ctx, 0.0, 1.0, 0.0, 1.0);
                CGContextFillRect(ctx, CGRectMake(x - (w/2) - 10, y + (hpHeight * (1.0f - hpPercentage)), hpWidth, hpHeight * hpPercentage));
            }
        }
    }
}
@end

@interface ViewController : UIViewController
@end

@implementation ViewController
- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = [UIColor colorWithRed:0.05 green:0.05 blue:0.08 alpha:1.0];
    
    float y = 50.0f;
    
    UILabel *title = [[UILabel alloc] initWithFrame:CGRectMake(20, y, 300, 40)];
    title.text = @"FF EXTERNAL iOS";
    title.textColor = [UIColor greenColor];
    [self.view addSubview:title];
    y += 60;
    
    // Toggle Aimbot
    UISwitch *aimSwitch = [[UISwitch alloc] initWithFrame:CGRectMake(20, y, 50, 30)];
    aimSwitch.on = g_Config.aimbotEnabled;
    [aimSwitch addTarget:self action:@selector(toggleAimbot:) forControlEvents:UIControlEventValueChanged];
    [self.view addSubview:aimSwitch];
    UILabel *aimLbl = [[UILabel alloc] initWithFrame:CGRectMake(80, y, 200, 30)];
    aimLbl.text = @"Aimbot";
    aimLbl.textColor = [UIColor whiteColor];
    [self.view addSubview:aimLbl];
    y += 50;

    // Toggle ESP
    UISwitch *espSwitch = [[UISwitch alloc] initWithFrame:CGRectMake(20, y, 50, 30)];
    espSwitch.on = g_Config.espEnabled;
    [espSwitch addTarget:self action:@selector(toggleESP:) forControlEvents:UIControlEventValueChanged];
    [self.view addSubview:espSwitch];
    UILabel *espLbl = [[UILabel alloc] initWithFrame:CGRectMake(80, y, 200, 30)];
    espLbl.text = @"ESP";
    espLbl.textColor = [UIColor whiteColor];
    [self.view addSubview:espLbl];
}
- (void)toggleAimbot:(UISwitch*)sender { g_Config.aimbotEnabled = sender.isOn; }
- (void)toggleESP:(UISwitch*)sender { g_Config.espEnabled = sender.isOn; }
@end

@interface AppDelegate : UIResponder <UIApplicationDelegate>
@property (strong, nonatomic) UIWindow *window;
@property (strong, nonatomic) UIWindow *overlayWindow;
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

    NSTimer* timer = [NSTimer scheduledTimerWithTimeInterval:1.0/60.0 repeats:YES block:^(NSTimer * _Nonnull timer) {
        [overlay setNeedsDisplay];
    }];
    [[NSRunLoop mainRunLoop] addTimer:timer forMode:NSRunLoopCommonModes];

    AVAudioSession *session = [AVAudioSession sharedInstance];
    [session setCategory:AVAudioSessionCategoryPlayback withOptions:AVAudioSessionCategoryOptionMixWithOthers error:nil];
    [session setActive:YES error:nil];

    std::thread([](){
        while (g_Running) {
            if (g_TaskPort == MACH_PORT_NULL) {
                g_TaskPort = get_task_port("FreeFire");
                if (g_TaskPort == MACH_PORT_NULL) g_TaskPort = get_task_port("UnityFramework");
                if (g_TaskPort != MACH_PORT_NULL) {
                    g_UnityBase = GetUnityBase();
                    std::thread(HackLoop).detach();
                    std::thread(AimbotThread).detach();
                    std::thread(EspThread).detach();
                }
            }
            std::this_thread::sleep_for(std::chrono::seconds(2));
        }
    }).detach();

    return YES;
}
@end

int main(int argc, char * argv[]) {
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
    }
}
