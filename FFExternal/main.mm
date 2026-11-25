#import <UIKit/UIKit.h>
#import <AVFoundation/AVFoundation.h>
#include <thread>
#include <chrono>
#include <cmath>

// Puxa as funções do Memory.cpp
extern mach_port_t get_task_port(const char* process_name);
extern mach_port_t g_TaskPort;

// Variáveis Globais
bool g_Aimbot = false;
bool g_AimHead = true;
bool g_AimNeck = false;
bool g_AimChest = false;
bool g_ESP = false;
bool g_ESPBox = true;
bool g_ESPHealth = true;
bool g_DrawFOV = false;
float g_AimFOV = 120.0f; // Padrão
bool g_Running = true;

// --- View do Overlay (Desenho) ---
@interface OverlayView : UIView
@end

@implementation OverlayView
- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        self.backgroundColor = [UIColor clearColor];
        self.userInteractionEnabled = YES;
        self.opaque = NO;
    }
    return self;
}

- (void)drawRect:(CGRect)rect {
    CGContextRef ctx = UIGraphicsGetCurrentContext();
    if (!ctx) return;

    float cx = rect.size.width / 2;
    float cy = rect.size.height / 2;

    // Desenha o Círculo do FOV
    if (g_DrawFOV) {
        CGContextSetRGBStrokeColor(ctx, 1.0, 1.0, 1.0, 0.5); // Branco translúcido
        CGContextSetLineWidth(ctx, 1.5);
        // O raio do círculo é baseado no g_AimFOV
        CGContextAddArc(ctx, cx, cy, g_AimFOV, 0, 2 * M_PI, 0);
        CGContextStrokePath(ctx);
    }

    // TODO: Desenhar ESP aqui (pega os dados da memória e desenha caixas)
}

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event {
    // Permite arrastar o menu se necessário
}
@end

// --- ViewController (O Menu) ---
@interface ViewController : UIViewController
@property (nonatomic, strong) UISlider *fovSlider;
@end

@implementation ViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = [UIColor colorWithRed:0.05 green:0.05 blue:0.08 alpha:0.9];
    
    UIScrollView *scrollView = [[UIScrollView alloc] initWithFrame:self.view.bounds];
    scrollView.contentSize = CGSizeMake(self.view.bounds.size.width, 800);
    [self.view addSubview:scrollView];

    UILabel *title = [[UILabel alloc] initWithFrame:CGRectMake(20, 50, 300, 40)];
    title.text = @"FF EXTERNAL HUD";
    title.textColor = [UIColor greenColor];
    title.font = [UIFont boldSystemFontOfSize:24];
    [scrollView addSubview:title];

    float y = 100.0f;
    float spacing = 45.0f;

    // --- AIMBOT SECTION ---
    UILabel *aimTitle = [[UILabel alloc] initWithFrame:CGRectMake(20, y, 200, 30)];
    aimTitle.text = @"--- Aimbot ---";
    aimTitle.textColor = [UIColor cyanColor];
    [scrollView addSubview:aimTitle];
    y += 35;

    UISwitch *aimToggle = [[UISwitch alloc] initWithFrame:CGRectMake(20, y, 50, 30)];
    [aimToggle addTarget:self action:@selector(toggleAimbot:) forControlEvents:UIControlEventValueChanged];
    [scrollView addSubview:aimToggle];
    UILabel *l1 = [[UILabel alloc] initWithFrame:CGRectMake(80, y, 200, 30)];
    l1.text = @"Ativar Aimbot";
    l1.textColor = [UIColor whiteColor];
    [scrollView addSubview:l1];
    y += spacing;

    UISwitch *headToggle = [[UISwitch alloc] initWithFrame:CGRectMake(40, y, 50, 30)];
    headToggle.on = YES;
    [headToggle addTarget:self action:@selector(toggleHead:) forControlEvents:UIControlEventValueChanged];
    [scrollView addSubview:headToggle];
    UILabel *l2 = [[UILabel alloc] initWithFrame:CGRectMake(100, y, 200, 30)];
    l2.text = @"Cabeça";
    l2.textColor = [UIColor whiteColor];
    [scrollView addSubview:l2];
    y += spacing;

    UISwitch *neckToggle = [[UISwitch alloc] initWithFrame:CGRectMake(40, y, 50, 30)];
    [neckToggle addTarget:self action:@selector(toggleNeck:) forControlEvents:UIControlEventValueChanged];
    [scrollView addSubview:neckToggle];
    UILabel *l3 = [[UILabel alloc] initWithFrame:CGRectMake(100, y, 200, 30)];
    l3.text = @"Pescoço";
    l3.textColor = [UIColor whiteColor];
    [scrollView addSubview:l3];
    y += spacing;

    UISwitch *chestToggle = [[UISwitch alloc] initWithFrame:CGRectMake(40, y, 50, 30)];
    [chestToggle addTarget:self action:@selector(toggleChest:) forControlEvents:UIControlEventValueChanged];
    [scrollView addSubview:chestToggle];
    UILabel *l4 = [[UILabel alloc] initWithFrame:CGRectMake(100, y, 200, 30)];
    l4.text = @"Peito";
    l4.textColor = [UIColor whiteColor];
    [scrollView addSubview:l4];
    y += spacing;

    // FOV Slider
    UILabel *l5 = [[UILabel alloc] initWithFrame:CGRectMake(20, y, 300, 30)];
    l5.text = [NSString stringWithFormat:@"Aim FOV (0-360): %.0f", g_AimFOV];
    l5.textColor = [UIColor whiteColor];
    l5.tag = 100;
    [scrollView addSubview:l5];
    y += 30;

    self.fovSlider = [[UISlider alloc] initWithFrame:CGRectMake(20, y, 300, 30)];
    self.fovSlider.minimumValue = 0;
    self.fovSlider.maximumValue = 360;
    self.fovSlider.value = g_AimFOV;
    [self.fovSlider addTarget:self action:@selector(fovChanged:) forControlEvents:UIControlEventValueChanged];
    [scrollView addSubview:self.fovSlider];
    y += spacing;

    UISwitch *fovCircleToggle = [[UISwitch alloc] initWithFrame:CGRectMake(20, y, 50, 30)];
    [fovCircleToggle addTarget:self action:@selector(toggleFovCircle:) forControlEvents:UIControlEventValueChanged];
    [scrollView addSubview:fovCircleToggle];
    UILabel *l6 = [[UILabel alloc] initWithFrame:CGRectMake(80, y, 200, 30)];
    l6.text = @"Mostrar Círculo FOV";
    l6.textColor = [UIColor whiteColor];
    [scrollView addSubview:l6];
    y += spacing + 15;

    // --- ESP SECTION ---
    UILabel *espTitle = [[UILabel alloc] initWithFrame:CGRectMake(20, y, 200, 30)];
    espTitle.text = @"--- ESP ---";
    espTitle.textColor = [UIColor cyanColor];
    [scrollView addSubview:espTitle];
    y += 35;

    UISwitch *espToggle = [[UISwitch alloc] initWithFrame:CGRectMake(20, y, 50, 30)];
    [espToggle addTarget:self action:@selector(toggleESP:) forControlEvents:UIControlEventValueChanged];
    [scrollView addSubview:espToggle];
    UILabel *l7 = [[UILabel alloc] initWithFrame:CGRectMake(80, y, 200, 30)];
    l7.text = @"Ativar ESP";
    l7.textColor = [UIColor whiteColor];
    [scrollView addSubview:l7];
    y += spacing;

    UISwitch *espBoxToggle = [[UISwitch alloc] initWithFrame:CGRectMake(40, y, 50, 30)];
    espBoxToggle.on = YES;
    [espBoxToggle addTarget:self action:@selector(toggleEspBox:) forControlEvents:UIControlEventValueChanged];
    [scrollView addSubview:espBoxToggle];
    UILabel *l8 = [[UILabel alloc] initWithFrame:CGRectMake(100, y, 200, 30)];
    l8.text = @"Caixa";
    l8.textColor = [UIColor whiteColor];
    [scrollView addSubview:l8];
    y += spacing;

    UISwitch *espHealthToggle = [[UISwitch alloc] initWithFrame:CGRectMake(40, y, 50, 30)];
    espHealthToggle.on = YES;
    [espHealthToggle addTarget:self action:@selector(toggleEspHealth:) forControlEvents:UIControlEventValueChanged];
    [scrollView addSubview:espHealthToggle];
    UILabel *l9 = [[UILabel alloc] initWithFrame:CGRectMake(100, y, 200, 30)];
    l9.text = @"Vida";
    l9.textColor = [UIColor whiteColor];
    [scrollView addSubview:l9];
}

- (void)toggleAimbot:(UISwitch*)sender { g_Aimbot = sender.isOn; }
- (void)toggleHead:(UISwitch*)sender { g_AimHead = sender.isOn; }
- (void)toggleNeck:(UISwitch*)sender { g_AimNeck = sender.isOn; }
- (void)toggleChest:(UISwitch*)sender { g_AimChest = sender.isOn; }
- (void)toggleESP:(UISwitch*)sender { g_ESP = sender.isOn; }
- (void)toggleEspBox:(UISwitch*)sender { g_ESPBox = sender.isOn; }
- (void)toggleEspHealth:(UISwitch*)sender { g_ESPHealth = sender.isOn; }
- (void)toggleFovCircle:(UISwitch*)sender { g_DrawFOV = sender.isOn; }

- (void)fovChanged:(UISlider*)sender {
    g_AimFOV = sender.value;
    UILabel *l5 = [self.view viewWithTag:100];
    l5.text = [NSString stringWithFormat:@"Aim FOV (0-360): %.0f", g_AimFOV];
}
@end

// --- AppDelegate (Gerencia a Janela Flutuante) ---
@interface AppDelegate : UIResponder <UIApplicationDelegate>
@property (strong, nonatomic) UIWindow *window;
@property (strong, nonatomic) UIWindow *overlayWindow;
@end

@implementation AppDelegate
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    
    // 1. Janela Normal (O Menu de Configurações)
    self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    self.window.rootViewController = [[ViewController alloc] init];
    [self.window makeKeyAndVisible];
    
    // 2. Janela Flutuante (O Overlay Transparente)
    self.overlayWindow = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    self.overlayWindow.windowLevel = UIWindowLevelAlert + 1; // Fica por cima
    self.overlayWindow.backgroundColor = [UIColor clearColor];
    self.overlayWindow.rootViewController = [[UIViewController alloc] init];
    self.overlayWindow.rootViewController.view.backgroundColor = [UIColor clearColor];
    
    OverlayView *overlay = [[OverlayView alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    [self.overlayWindow.rootViewController.view addSubview:overlay];
    self.overlayWindow.hidden = NO;

    // Loop pra forçar o desenho do FOV
    NSTimer* timer = [NSTimer scheduledTimerWithTimeInterval:1.0/60.0 repeats:YES block:^(NSTimer * _Nonnull timer) {
        [overlay setNeedsDisplay];
    }];
    [[NSRunLoop mainRunLoop] addTimer:timer forMode:NSRunLoopCommonModes];

    // Configura áudio em background
    AVAudioSession *session = [AVAudioSession sharedInstance];
    [session setCategory:AVAudioSessionCategoryPlayback withOptions:AVAudioSessionCategoryOptionMixWithOthers error:nil];
    [session setActive:YES error:nil];

    return YES;
}
@end

// --- Função Main ---
int main(int argc, char * argv[]) {
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
    }
}
