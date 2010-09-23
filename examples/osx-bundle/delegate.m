
#import "delegate.h"

@implementation MinimalAppAppDelegate

@synthesize window;
@synthesize button;

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
	// Insert code here to initialize your application
}

- (IBAction) buttonClicked:(id)sender {
	NSRunAlertPanel(@"Hello, world",
					@"Hello",
					@"OK", nil, nil);
}

@end
