#import <UIKit/UIKit.h>
#import <AVFoundation/AVFoundation.h>
#include <thread>
#include <chrono>
#include <cmath>

// Puxa as funções do Memory.cpp
extern mach_port_t get_task_port(const char* process_name);
extern mach_port_t g_TaskPort;

// Variáveis de controle
bool g_Aimbot = false;
bool g_ESP = false;
bool g_Running = true;

// Loop pra achar o processo do Free Fire
void MemoryThread() {
    while (g_Running) {
        if (g_TaskPort == MACH_PORT_NULL) {
            // Tenta achar pelo nome do executável
            g_TaskPort = get_task_port("FreeFire");
            
            // Se não achar, tenta pelo nome do framework
            if (g_TaskPort == MACH_PORT_NULL) {
                g_TaskPort = get_task_port("UnityFramework");
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
}

// --- ViewController (O Menu do App) ---
@interface ViewController : UIViewController
@property (nonatomic, strong) UISwitch *aimbotSwitch;
@property (nonatomic, strong) UISwitch *espSwitch;
@end

@implementation ViewController
- (void)viewDidLoad {
    [super viewDidLoad];
    self.view.backgroundColor = [UIColor colorWithRed:0.05 green:0.05 blue:0.08 alpha:1.0];
    
    UILabel *title = [[UILabel alloc] initWithFrame:CGRectMake(20, 100, 300, 40)];
    title.text = @"FF EXTERNAL MENU";
    title.textColor = [UIColor greenColor];
    title.font = [UIFont boldSystemFontOfSize:24];
    [self.view addSubview:title];

    // Aimbot Switch
    self.aimbotSwitch = [[UISwitch alloc] initWithFrame:CGRectMake(20, 160, 50, 30)];
    [self.aimbotSwitch addTarget:self action:@selector(toggleAimbot:) forControlEvents:UIControlEventValueChanged];
    [self.view addSubview:self.aimbotSwitch];
    
    UILabel *l1 = [[UILabel alloc] initWithFrame:CGRectMake(80, 160, 200, 30)];
    l1.text = @"Aimbot Ativado";
    l1.textColor = [UIColor whiteColor];
    [self.view addSubview:l1];

    // ESP Switch
    self.espSwitch = [[UISwitch alloc] initWithFrame:CGRectMake(20, 220, 50, 30)];
    [self.espSwitch addTarget:self action:@selector(toggleESP:) forControlEvents:UIControlEventValueChanged];
    [self.view addSubview:self.espSwitch];
    
    UILabel *l2 = [[UILabel alloc] initWithFrame:CGRectMake(80, 220, 200, 30)];
    l2.text = @"ESP Caixa e Vida";
    l2.textColor = [UIColor whiteColor];
    [self.view addSubview:l2];
}

- (void)toggleAimbot:(UISwitch*)sender { g_Aimbot = sender.isOn; }
- (void)toggleESP:(UISwitch*)sender { g_ESP = sender.isOn; }
@end

// --- AppDelegate (O Ciclo de Vida do App) ---
@interface AppDelegate : UIResponder <UIApplicationDelegate>
@property (strong, nonatomic) UIWindow *window;
@end

@implementation AppDelegate
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    // Configura a janela principal
    self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    self.window.rootViewController = [[ViewController alloc] init];
    [self.window makeKeyAndVisible];
    
    // Configura o áudio em background pra não morrer quando minimizar
    AVAudioSession *session = [AVAudioSession sharedInstance];
    [session setCategory:AVAudioSessionCategoryPlayback withOptions:AVAudioSessionCategoryOptionMixWithOthers error:nil];
    [session setActive:YES error:nil];
    
    // Inicia a thread de leitura de memória
    std::thread(MemoryThread).detach();

    return YES;
}
@end

// --- Função Main (Obrigatória) ---
int main(int argc, char * argv[]) {
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
    }
}
