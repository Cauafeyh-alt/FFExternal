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
    
    Config cfg = GetConfig();

    if (cfg.drawFOV) {
        CGContextSetRGBStrokeColor(ctx, 1.0, 1.0, 1.0, 0.5);
        CGContextSetLineWidth(ctx, 1.5);
        CGContextAddArc(ctx, cx, cy, cfg.aimFOV, 0, 2 * M_PI, 0);
        CGContextStrokePath(ctx);
    }

    if (cfg.espEnabled) {
        std::lock_guard<std::mutex> lk(g_EspMutex);
        for (const auto& e : g_EspEntities) {
            float x = e.headPos.x;
            float y = e.headPos.y;
            float w = 50.0f;
            float h = e.footPos.y - y;

            if (h > 0 && h < 1000) { 
                if (cfg.espBox) {
                    CGContextSetRGBStrokeColor(ctx, 1.0, 0.0, 0.0, 1.0);
                    CGContextSetLineWidth(ctx, 2.0);
                    CGContextStrokeRect(ctx, CGRectMake(x - (w/2), y, w, h));
                }

                if (cfg.espHealth) {
                    float hpWidth = 5.0f;
                    float hpPercentage = e.health / 100.0f; 
                    if (hpPercentage > 1.0f) hpPercentage = 1.0f;
                    if (hpPercentage < 0.0f) hpPercentage = 0.0f;
                    
                    CGContextSetRGBFillColor(ctx, 0.0, 1.0, 0.0, 1.0);
                    CGContextFillRect(ctx, CGRectMake(x - (w/2) - 10, y + (h * (1.0f - hpPercentage)), hpWidth, h * hpPercentage));
                }
            }
        }
    }
}
@end

@interface ViewController : UIViewController
@property (nonatomic, strong) UILabel *debugLabel;
@property (nonatomic, strong) UILabel *fovLabel;
@end

@implementation ViewController
- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = [UIColor colorWithRed:0.05 green:0.05 blue:0.08 alpha:1.0];
    
    UIScrollView *scroll = [[UIScrollView alloc] initWithFrame:self.view.bounds];
    scroll.contentSize = CGSizeMake(self.view.bounds.size.width, 800);
    [self.view addSubview:scroll];

    float y = 50.0f;
    float spacing = 45.0f;

    UILabel *title = [[UILabel alloc] initWithFrame:CGRectMake(20, y, 300, 40)];
    title.text = @"FF EXTERNAL HUD";
    title.textColor = [UIColor greenColor];
    title.font = [UIFont boldSystemFontOfSize:24];
    [scroll addSubview:title];
    y += 50;

    self.debugLabel = [[UILabel alloc] initWithFrame:CGRectMake(20, y, 350, 40)];
    self.debugLabel.text = @"Status: Iniciando...";
    self.debugLabel.textColor = [UIColor yellowColor];
    self.debugLabel.numberOfLines = 2;
    self.debugLabel.font = [UIFont monospacedSystemFontOfSize:10 weight:UIFontWeightRegular];
    [scroll addSubview:self.debugLabel];
    y += 50;

    UILabel *aimTitle = [[UILabel alloc] initWithFrame:CGRectMake(20, y, 200, 30)];
    aimTitle.text = @"--- Aimbot ---";
    aimTitle.textColor = [UIColor cyanColor];
    [scroll addSubview:aimTitle];
    y += 35;

    Config cfg = GetConfig();

    UISwitch *aimToggle = [[UISwitch alloc] initWithFrame:CGRectMake(20, y, 50, 30)];
    aimToggle.on = cfg.aimbotEnabled;
    [aimToggle addTarget:self action:@selector(toggleAimbot:) forControlEvents:UIControlEventValueChanged];
    [scroll addSubview:aimToggle];
    UILabel *l1 = [[UILabel alloc] initWithFrame:CGRectMake(80, y, 200, 30)];
    l1.text = @"Ativar Aimbot"; l1.textColor = [UIColor whiteColor];
    [scroll addSubview:l1];
    y += spacing;

    UISwitch *headToggle = [[UISwitch alloc] initWithFrame:CGRectMake(40, y, 50, 30)];
    headToggle.on = cfg.aimHead;
    [headToggle addTarget:self action:@selector(toggleHead:) forControlEvents:UIControlEventValueChanged];
    [scroll addSubview:headToggle];
    UILabel *l2 = [[UILabel alloc] initWithFrame:CGRectMake(100, y, 200, 30)];
    l2.text = @"Cabeca"; l2.textColor = [UIColor whiteColor];
    [scroll addSubview:l2];
    y += spacing;

    UISwitch *neckToggle = [[UISwitch alloc] initWithFrame:CGRectMake(40, y, 50, 30)];
    neckToggle.on = cfg.aimNeck;
    [neckToggle addTarget:self action:@selector(toggleNeck:) forControlEvents:UIControlEventValueChanged];
    [scroll addSubview:neckToggle];
    UILabel *l3 = [[UILabel alloc] initWithFrame:CGRectMake(100, y, 200, 30)];
    l3.text = @"Pescoco"; l3.textColor = [UIColor whiteColor];
    [scroll addSubview:l3];
    y += spacing;

    UISwitch *chestToggle = [[UISwitch alloc] initWithFrame:CGRectMake(40, y, 50, 30)];
    chestToggle.on = cfg.aimChest;
    [chestToggle addTarget:self action:@selector(toggleChest:) forControlEvents:UIControlEventValueChanged];
    [scroll addSubview:chestToggle];
    UILabel *l4 = [[UILabel alloc] initWithFrame:CGRectMake(100, y, 200, 30)];
    l4.text = @"Peito"; l4.textColor = [UIColor whiteColor];
    [scroll addSubview:l4];
    y += spacing;

    self.fovLabel = [[UILabel alloc] initWithFrame:CGRectMake(20, y, 300, 30)];
    self.fovLabel.text = [NSString stringWithFormat:@"Aim FOV (0-360): %.0f", cfg.aimFOV];
    self.fovLabel.textColor = [UIColor whiteColor];
    [scroll addSubview:self.fovLabel];
    y += 30;

    UISlider *fovSlider = [[UISlider alloc] initWithFrame:CGRectMake(20, y, 300, 30)];
    fovSlider.minimumValue = 0; fovSlider.maximumValue = 360;
    fovSlider.value = cfg.aimFOV;
    [fovSlider addTarget:self action:@selector(fovChanged:) forControlEvents:UIControlEventValueChanged];
    [scroll addSubview:fovSlider];
    y += spacing;

    UISwitch *fovCircleToggle = [[UISwitch alloc] initWithFrame:CGRectMake(20, y, 50, 30)];
    fovCircleToggle.on = cfg.drawFOV;
    [fovCircleToggle addTarget:self action:@selector(toggleFovCircle:) forControlEvents:UIControlEventValueChanged];
    [scroll addSubview:fovCircleToggle];
    UILabel *l6 = [[UILabel alloc] initWithFrame:CGRectMake(80, y, 200, 30)];
    l6.text = @"Mostrar Circulo FOV"; l6.textColor = [UIColor whiteColor];
    [scroll addSubview:l6];
    y += spacing + 15;

    UILabel *espTitle = [[UILabel alloc] initWithFrame:CGRectMake(20, y, 200, 30)];
    espTitle.text = @"--- ESP ---";
    espTitle.textColor = [UIColor cyanColor];
    [scroll addSubview:espTitle];
    y += 35;

    UISwitch *espToggle = [[UISwitch alloc] initWithFrame:CGRectMake(20, y, 50, 30)];
    espToggle.on = cfg.espEnabled;
    [espToggle addTarget:self action:@selector(toggleESP:) forControlEvents:UIControlEventValueChanged];
    [scroll addSubview:espToggle];
    UILabel *l7 = [[UILabel alloc] initWithFrame:CGRectMake(80, y, 200, 30)];
    l7.text = @"Ativar ESP"; l7.textColor = [UIColor whiteColor];
    [scroll addSubview:l7];
    y += spacing;

    UISwitch *espBoxToggle = [[UISwitch alloc] initWithFrame:CGRectMake(40, y, 50, 30)];
    espBoxToggle.on = cfg.espBox;
    [espBoxToggle addTarget:self action:@selector(toggleEspBox:) forControlEvents:UIControlEventValueChanged];
    [scroll addSubview:espBoxToggle];
    UILabel *l8 = [[UILabel alloc] initWithFrame:CGRectMake(100, y, 200, 30)];
    l8.text = @"Caixa"; l8.textColor = [UIColor whiteColor];
    [scroll addSubview:l8];
    y += spacing;

    UISwitch *espHealthToggle = [[UISwitch alloc] initWithFrame:CGRectMake(40, y, 50, 30)];
    espHealthToggle.on = cfg.espHealth;
    [espHealthToggle addTarget:self action:@selector(toggleEspHealth:) forControlEvents:UIControlEventValueChanged];
    [scroll addSubview:espHealthToggle];
    UILabel *l9 = [[UILabel alloc] initWithFrame:CGRectMake(100, y, 200, 30)];
    l9.text = @"Vida"; l9.textColor = [UIColor whiteColor];
    [scroll addSubview:l9];

    [NSTimer scheduledTimerWithTimeInterval:0.5 repeats:YES block:^(NSTimer * _Nonnull timer) {
        dispatch_async(dispatch_get_main_queue(), ^{
            std::string safeStr;
            {
                std::lock_guard<std::mutex> lk(g_PlayerMutex);
                safeStr = g_DebugInfo;
            }
            self.debugLabel.text = [NSString stringWithUTF8String:safeStr.c_str()];
        });
    }];
}

- (void)toggleAimbot:(UISwitch*)s { UpdateConfig([s](Config& c){ c.aimbotEnabled = s.isOn; }); }
- (void)toggleHead:(UISwitch*)s { UpdateConfig([s](Config& c){ c.aimHead = s.isOn; }); }
- (void)toggleNeck:(UISwitch*)s { UpdateConfig([s](Config& c){ c.aimNeck = s.isOn; }); }
- (void)toggleChest:(UISwitch*)s { UpdateConfig([s](Config& c){ c.aimChest = s.isOn; }); }
- (void)toggleESP:(UISwitch*)s { UpdateConfig([s](Config& c){ c.espEnabled = s.isOn; }); }
- (void)toggleEspBox:(UISwitch*)s { UpdateConfig([s](Config& c){ c.espBox = s.isOn; }); }
- (void)toggleEspHealth:(UISwitch*)s { UpdateConfig([s](Config& c){ c.espHealth = s.isOn; }); }
- (void)toggleFovCircle:(UISwitch*)s { UpdateConfig([s](Config& c){ c.drawFOV = s.isOn; }); }

- (void)fovChanged:(UISlider*)s {
    UpdateConfig([s](Config& c){ c.aimFOV = s.value; });
    self.fovLabel.text = [NSString stringWithFormat:@"Aim FOV (0-360): %.0f", s.value];
}
@end

@interface AppDelegate : UIResponder <UIApplicationDelegate> {
    std::thread _hackThread;
    std::thread _aimbotThread;
    std::thread _espThread;
}
@property (strong, nonatomic) UIWindow *window;
@property (strong, nonatomic) UIWindow *overlayWindow;
@property (strong, nonatomic) CADisplayLink *displayLink;
@end

@implementation AppDelegate
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    
    CGSize screenSize = [UIScreen mainScreen].bounds.size;
    g_ScreenWidth = (int)MAX(screenSize.width, screenSize.height);
    g_ScreenHeight = (int)MIN(screenSize.width, screenSize.height);

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
    
    NSString *logPath = [NSString stringWithFormat:@"%@/ff_log.txt", documentsDirectory];
    set_log_path([logPath UTF8String]);

    _hackThread = std::thread(HackLoop);
    _aimbotThread = std::thread(AimbotThread);
    _espThread = std::thread(EspThread);

    return YES;
}

- (void)applicationWillTerminate:(UIApplication *)application {
    g_Running.store(false, std::memory_order_release);
    
    // Correção da captura: O Clang do Linux não aceita [this] no ObjC++.
    // Pegamos os ponteiros diretos pras variáveis membro.
    std::thread* h = &_hackThread;
    std::thread* a = &_aimbotThread;
    std::thread* e = &_espThread;
    
    std::thread([h, a, e]() {
        if (h->joinable()) h->join();
        if (a->joinable()) a->join();
        if (e->joinable()) e->join();
        Log("[+] Threads encerradas pelo coordenador.");
    }).detach();
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
