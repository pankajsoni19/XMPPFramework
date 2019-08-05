#import <Foundation/Foundation.h>
#import "GCDMulticastDelegate.h"
#import "CocoaAsyncSocket/GCDAsyncSocket.h"

@import KissXML;

@class XMPPSRVResolver;
@class XMPPParser;
@class XMPPJID;
@class XMPPIQ;
@class XMPPMessage;
@class XMPPPresence;
@class XMPPModule;
@class XMPPElement;
@class XMPPElementReceipt;
@protocol XMPPStreamDelegate;

#if TARGET_OS_IPHONE
  #define MIN_KEEPALIVE_INTERVAL      20.0 // 20 Seconds
  #define DEFAULT_KEEPALIVE_INTERVAL 120.0 //  2 Minutes
#else
  #define MIN_KEEPALIVE_INTERVAL      10.0 // 10 Seconds
  #define DEFAULT_KEEPALIVE_INTERVAL 300.0 //  5 Minutes
#endif

extern NSString *const XMPPStreamErrorDomain;

typedef NS_ENUM(NSUInteger, XMPPStreamErrorCode) {
	XMPPStreamInvalidType,       // Attempting to access P2P methods in a non-P2P stream, or vice-versa
	XMPPStreamInvalidState,      // Invalid state for requested action, such as connect when already connected
	XMPPStreamInvalidProperty,   // Missing a required property, such as myJID
	XMPPStreamInvalidParameter,  // Invalid parameter, such as a nil JID
	XMPPStreamUnsupportedAction, // The server doesn't support the requested action
};

extern const NSTimeInterval XMPPStreamTimeoutNone;

@interface XMPPStream : NSObject <GCDAsyncSocketDelegate>

/**
 * Standard XMPP initialization.
 * The stream is a standard client to server connection.
 * 
 * P2P streams using XEP-0174 are also supported.
 * See the P2P section below.
**/
- (id)init;

/**
 * XMPPStream uses a multicast delegate.
 * This allows one to add multiple delegates to a single XMPPStream instance,
 * which makes it easier to separate various components and extensions.
 * 
 * For example, if you were implementing two different custom extensions on top of XMPP,
 * you could put them in separate classes, and simply add each as a delegate.
**/
- (void)addDelegate:(id)delegate delegateQueue:(dispatch_queue_t)delegateQueue;
- (void)removeDelegate:(id)delegate delegateQueue:(dispatch_queue_t)delegateQueue;
- (void)removeDelegate:(id)delegate;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark Properties
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * The server's hostname that should be used to make the TCP connection.
 * This may be a domain name (e.g. "deusty.com") or an IP address (e.g. "70.85.193.226").
 * 
 * Note that this may be different from the virtual xmpp hostname.
 * Just as HTTP servers can support mulitple virtual hosts from a single server, so too can xmpp servers.
 * A prime example is google via google apps.
 * 
 * For example, say you own the domain "mydomain.com".
 * If you go to mydomain.com in a web browser,
 * you are directed to your apache server running on your webserver somewhere in the cloud.
 * But you use google apps for your email and xmpp needs.
 * So if somebody sends you an email, it actually goes to google's servers, where you later access it from.
 * Similarly, you connect to google's servers to sign into xmpp.
 * 
 * In the example above, your hostname is "talk.google.com" and your JID is "me@mydomain.com".
 * 
 * This hostName property is optional.
 * If you do not set the hostName, then the framework will follow the xmpp specification using jid's domain.
 * That is, it first do an SRV lookup (as specified in the xmpp RFC).
 * If that fails, it will fall back to simply attempting to connect to the jid's domain.
**/
@property (readwrite, copy) NSString *hostName;

/**
 * The port the xmpp server is running on.
 * If you do not explicitly set the port, the default port will be used.
 * If you set the port to zero, the default port will be used.
 * 
 * The default port is 5222.
**/
@property (readwrite, assign) UInt16 hostPort;

/**
 * The JID of the user.
 * 
 * This value is required, and is used in many parts of the underlying implementation.
 * When connecting, the domain of the JID is used to properly specify the correct xmpp virtual host.
 * It is used during registration to supply the username of the user to create an account for.
 * It is used during authentication to supply the username of the user to authenticate with.
 * And the resource may be used post-authentication during the required xmpp resource binding step.
 * 
 * A proper JID is of the form user@domain/resource.
 * For example: robbiehanson@deusty.com/work
 * 
 * The resource is optional, in the sense that if one is not supplied,
 * one will be automatically generated for you (either by us or by the server).
 * 
 * Please note:
 * Resource collisions are handled in different ways depending on server configuration.
 * 
 * For example:
 * You are signed in with user1@domain.com/home on your desktop.
 * Then you attempt to sign in with user1@domain.com/home on your laptop.
 * 
 * The server could possibly:
 * - Reject the resource request for the laptop.
 * - Accept the resource request for the laptop, and immediately disconnect the desktop.
 * - Automatically assign the laptop another resource without a conflict.
 * 
 * For this reason, you may wish to check the myJID variable after the stream has been connected,
 * just in case the resource was changed by the server.
**/
@property (readwrite, copy) XMPPJID *myJID;
@property (readwrite, copy) NSString *password;
@property (readwrite, assign) BOOL shouldSendInitialPresence;

/**
 * Many routers will teardown a socket mapping if there is no activity on the socket.
 * For this reason, the xmpp stream supports sending keep-alive data.
 * This is simply whitespace, which is ignored by the xmpp protocol.
 * 
 * Keep-alive data is only sent in the absence of any other data being sent/received.
 * 
 * The default value is defined in DEFAULT_KEEPALIVE_INTERVAL.
 * The minimum value is defined in MIN_KEEPALIVE_INTERVAL.
 * 
 * To disable keep-alive, set the interval to zero (or any non-positive number).
 * 
 * The keep-alive timer (if enabled) fires every (keepAliveInterval / 4) seconds.
 * Upon firing it checks when data was last sent/received,
 * and sends keep-alive data if the elapsed time has exceeded the keepAliveInterval.
 * Thus the effective resolution of the keepalive timer is based on the interval.
 * 
 * @see keepAliveWhitespaceCharacter
**/
@property (readwrite, assign) NSTimeInterval keepAliveInterval;

/**
 * The keep-alive mechanism sends whitespace which is ignored by the xmpp protocol.
 * The default whitespace character is a space (' ').
 * 
 * This can be changed, for whatever reason, to another whitespace character.
 * Valid whitespace characters are space(' '), tab('\t') and newline('\n').
 * 
 * If you attempt to set the character to any non-whitespace character, the attempt is ignored.
 * 
 * @see keepAliveInterval
**/
@property (readwrite, assign) char keepAliveWhitespaceCharacter;

/**
 * Represents the last sent presence element concerning the presence of myJID on the server.
 * In other words, it represents the presence as others see us.
 * 
 * This excludes presence elements sent concerning subscriptions, MUC rooms, etc.
 * 
 * @see resendMyPresence
**/
@property (strong, readonly) XMPPPresence *myPresence;

/**
 * Returns the total number of bytes bytes sent/received by the xmpp stream.
 * 
 * By default this is the byte count since the xmpp stream object has been created.
 * If the stream has connected/disconnected/reconnected multiple times,
 * the count will be the summation of all connections.
 * 
 * The functionality may optionaly be changed to count only the current socket connection.
 * @see resetByteCountPerConnection
**/
@property (readonly) uint64_t numberOfBytesSent;
@property (readonly) uint64_t numberOfBytesReceived;

/**
 * Same as the individual properties,
 * but provides a way to fetch them in one atomic operation.
**/
- (void)getNumberOfBytesSent:(uint64_t *)bytesSentPtr numberOfBytesReceived:(uint64_t *)bytesReceivedPtr;

/**
 * Affects the funtionality of the byte counter.
 * 
 * The default value is NO.
 * 
 * If set to YES, the byte count will be reset just prior to a new connection (in the connect methods).
**/
@property (readwrite, assign) BOOL resetByteCountPerConnection;

/**
 * The tag property allows you to associate user defined information with the stream.
 * Tag values are not used internally, and should not be used by xmpp modules.
**/
@property (readwrite, strong) id tag;

/**
 * Validates that a response element is FROM the jid that the request element was sent TO.
 * Supports validating responses when request didn't specify a TO.
 *
 * @see isValidResponseElementFrom:forRequestElementTo:
 * @see isValidResponseElement:forRequestElement:
 *
 * The default value is NO.
**/
@property (readwrite, assign) BOOL validatesResponses;

#if TARGET_OS_IPHONE

/**
 * If set, the kCFStreamNetworkServiceTypeVoIP flags will be set on the underlying CFRead/Write streams.
 * 
 * The default value is NO.
**/
@property (readwrite, assign) BOOL enableBackgroundingOnSocket;

#endif

/**
 * By default, IPv6 is now preferred over IPv4 to satisfy Apple's June 2016
 * DNS64/NAT64 requirements for app approval. Disabling this option may cause
 * issues during app approval.
 *
 * https://developer.apple.com/library/ios/documentation/NetworkingInternetWeb/Conceptual/NetworkingOverview/UnderstandingandPreparingfortheIPv6Transition/UnderstandingandPreparingfortheIPv6Transition.html
 *
 * This new default may cause connectivity issues for misconfigured servers that have
 * both A and AAAA DNS records but don't respond to IPv6. A proper solution to this
 * is to fix your XMPP server and/or DNS entries. However, when Happy Eyeballs
 * (RFC 6555) is implemented upstream in GCDAsyncSocket it should resolve the issue 
 * of misconfigured servers because it will try both the preferred protocol (IPv6) and
 * and fallback protocol (IPv4) after a 300ms delay.
 *
 * Any changes to this option MUST be done before calling connect.
 *
 * The default value is YES.
 **/
@property (assign, readwrite) BOOL preferIPv6;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark State
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Returns YES if the connection is closed, and thus no stream is open.
 * If the stream is neither disconnected, nor connected, then a connection is currently being established.
**/
- (BOOL)isDisconnected;

/**
 * Returns YES is the connection is currently connecting
**/
- (BOOL)isConnecting;

/**
 * Returns YES if the connection is open, and the stream has been properly established.
 * If the stream is neither disconnected, nor connected, then a connection is currently being established.
 * 
 * If this method returns YES, then it is ready for you to start sending and receiving elements.
**/
- (BOOL)isConnected;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark Connect & Disconnect
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Connects to the configured hostName on the configured hostPort.
 * The timeout is optional. To not time out use XMPPStreamTimeoutNone.
 * If the hostName or myJID are not set, this method will return NO and set the error parameter.
**/
- (BOOL)connectWithTimeout:(NSTimeInterval)timeout error:(NSError **)errPtr;

/**
 * Disconnects from the remote host by closing the underlying TCP socket connection.
 * The terminating </stream:stream> element is not sent to the server.
 * 
 * This method is synchronous.
 * Meaning that the disconnect will happen immediately, even if there are pending elements yet to be sent.
 * 
 * The xmppStreamDidDisconnect:withError: delegate method will immediately be dispatched onto the delegate queue.
**/
- (void)disconnect;

/**
 * Disconnects from the remote host by sending the terminating </stream:stream> element,
 * and then closing the underlying TCP socket connection.
 * 
 * This method is asynchronous.
 * The disconnect will happen after all pending elements have been sent.
 * Attempting to send elements after this method has been called will not work (the elements won't get sent).
**/
- (void)disconnectAfterSending;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark Authentication
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Returns whether or not the xmpp stream has successfully authenticated with the server.
**/
- (BOOL)isAuthenticated;

/**
 * Returns the date when the xmpp stream successfully authenticated with the server.
 **/
- (NSDate *)authenticationDate;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark Server Info
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * This method will return the root element of the document.
 * This element contains the opening <stream:stream/> and <stream:features/> tags received from the server.
 * 
 * If multiple <stream:features/> have been received during the course of stream negotiation,
 * the root element contains only the most recent (current) version.
 * 
 * Note: The rootElement is "empty", in-so-far as it does not contain all the XML elements the stream has
 * received during it's connection. This is done for performance reasons and for the obvious benefit
 * of being more memory efficient.
**/
- (NSXMLElement *)rootElement;


////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark Sending
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Sends the given XML element.
 * If the stream is not yet connected, this method does nothing.
**/
- (void)sendElement:(NSXMLElement *)element;

/**
 * Just like the sendElement: method above,
 * but allows you to receive a receipt that can later be used to verify the element has been sent.
 * 
 * If you later want to check to see if the element has been sent:
 * 
 * if ([receipt wait:0]) {
 *   // Element has been sent
 * }
 * 
 * If you later want to wait until the element has been sent:
 * 
 * if ([receipt wait:-1]) {
 *   // Element was sent
 * } else {
 *   // Element failed to send due to disconnection
 * }
 * 
 * It is important to understand what it means when [receipt wait:timeout] returns YES.
 * It does NOT mean the server has received the element.
 * It only means the data has been queued for sending in the underlying OS socket buffer.
 * 
 * So at this point the OS will do everything in its capacity to send the data to the server,
 * which generally means the server will eventually receive the data.
 * Unless, of course, something horrible happens such as a network failure,
 * or a system crash, or the server crashes, etc.
 * 
 * Even if you close the xmpp stream after this point, the OS will still do everything it can to send the data.
**/
- (void)sendElement:(NSXMLElement *)element andGetReceipt:(XMPPElementReceipt **)receiptPtr;

/**
 * Fetches and resends the myPresence element (if available) in a single atomic operation.
 * 
 * There are various xmpp extensions that hook into the xmpp stream and append information to outgoing presence stanzas.
 * For example, the XMPPCapabilities module automatically appends capabilities information (as a hash).
 * When these modules need to update/change their appended information,
 * they should use this method to do so.
 * 
 * The alternative is to fetch the myPresence element, and resend it manually using the sendElement method.
 * However, that is 2 seperate operations, and the user, may send a different presence element inbetween.
 * Using this method guarantees everything is done as an atomic operation.
**/
- (void)resendMyPresence;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark Stanza Validation
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Validates that a response element is FROM the jid that the request element was sent TO.
 * Supports validating responses when request didn't specify a TO.
**/
- (BOOL)isValidResponseElementFrom:(XMPPJID *)from forRequestElementTo:(XMPPJID *)to;

- (BOOL)isValidResponseElement:(XMPPElement *)response forRequestElement:(XMPPElement *)request;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark Module Plug-In System
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * The XMPPModule class automatically invokes these methods when it is activated/deactivated.
 * 
 * The registerModule method registers the module with the xmppStream.
 * If there are any other modules that have requested to be automatically added as delegates to modules of this type,
 * then those modules are automatically added as delegates during the asynchronous execution of this method.
 * 
 * The registerModule method is asynchronous.
 * 
 * The unregisterModule method unregisters the module with the xmppStream,
 * and automatically removes it as a delegate of any other module.
 * 
 * The unregisterModule method is fully synchronous.
 * That is, after this method returns, the module will not be scheduled in any more delegate calls from other modules.
 * However, if the module was already scheduled in an existing asynchronous delegate call from another module,
 * the scheduled delegate invocation remains queued and will fire in the near future.
 * Since the delegate invocation is already queued,
 * the module's retainCount has been incremented,
 * and the module will not be deallocated until after the delegate invocation has fired.
**/
- (void)registerModule:(XMPPModule *)module;
- (void)unregisterModule:(XMPPModule *)module;

/**
 * Automatically registers the given delegate with all current and future registered modules of the given class.
 * 
 * That is, the given delegate will be added to the delegate list ([module addDelegate:delegate delegateQueue:dq]) to
 * all current and future registered modules that respond YES to [module isKindOfClass:aClass].
 * 
 * This method is used by modules to automatically integrate with other modules.
 * For example, a module may auto-add itself as a delegate to XMPPCapabilities
 * so that it can broadcast its implemented features.
 * 
 * This may also be useful to clients, for example, to add a delegate to instances of something like XMPPChatRoom,
 * where there may be multiple instances of the module that get created during the course of an xmpp session.
 * 
 * If you auto register on multiple queues, you can remove all registrations with a single
 * call to removeAutoDelegate::: by passing NULL as the 'dq' parameter.
 * 
 * If you auto register for multiple classes, you can remove all registrations with a single
 * call to removeAutoDelegate::: by passing nil as the 'aClass' parameter.
**/
- (void)autoAddDelegate:(id)delegate delegateQueue:(dispatch_queue_t)delegateQueue toModulesOfClass:(Class)aClass;
- (void)removeAutoDelegate:(id)delegate delegateQueue:(dispatch_queue_t)delegateQueue fromModulesOfClass:(Class)aClass;

/**
 * Allows for enumeration of the currently registered modules.
 * 
 * This may be useful if the stream needs to be queried for modules of a particular type.
**/
- (void)enumerateModulesWithBlock:(void (^)(XMPPModule *module, NSUInteger idx, BOOL *stop))block;

/**
 * Allows for enumeration of the currently registered modules that are a kind of Class.
 * idx is in relation to all modules not just those of the given class.
**/
- (void)enumerateModulesOfClass:(Class)aClass withBlock:(void (^)(XMPPModule *module, NSUInteger idx, BOOL *stop))block;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark Utilities
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Generates and returns a new autoreleased UUID.
 * UUIDs (Universally Unique Identifiers) may also be known as GUIDs (Globally Unique Identifiers).
 * 
 * The UUID is generated using the CFUUID library, which generates a unique 128 bit value.
 * The uuid is then translated into a string using the standard format for UUIDs:
 * "68753A44-4D6F-1226-9C60-0050E4C00067"
 * 
 * This method is most commonly used to generate a unique id value for an xmpp element.
**/
+ (NSString *)generateUUID;
- (NSString *)generateUUID;

@end

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark -
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

@interface XMPPElementReceipt : NSObject
{
	uint32_t atomicFlags;
	dispatch_semaphore_t semaphore;
}

/**
 * Element receipts allow you to check to see if the element has been sent.
 * The timeout parameter allows you to do any of the following:
 * 
 * - Do an instantaneous check (pass timeout == 0)
 * - Wait until the element has been sent (pass timeout < 0)
 * - Wait up to a certain amount of time (pass timeout > 0)
 * 
 * It is important to understand what it means when [receipt wait:timeout] returns YES.
 * It does NOT mean the server has received the element.
 * It only means the data has been queued for sending in the underlying OS socket buffer.
 * 
 * So at this point the OS will do everything in its capacity to send the data to the server,
 * which generally means the server will eventually receive the data.
 * Unless, of course, something horrible happens such as a network failure,
 * or a system crash, or the server crashes, etc.
 * 
 * Even if you close the xmpp stream after this point, the OS will still do everything it can to send the data.
**/
- (BOOL)wait:(NSTimeInterval)timeout;

@end

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma mark -
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

@protocol XMPPStreamDelegate
@optional

/**
 * This method is called before the stream begins the connection process.
 *
 * If developing an iOS app that runs in the background, this may be a good place to indicate
 * that this is a task that needs to continue running in the background.
**/
- (void)xmppStreamWillConnect:(XMPPStream *)sender;

/**
 * This method is called after the tcp socket has connected to the remote host.
 * It may be used as a hook for various things, such as updating the UI or extracting the server's IP address.
 * 
 * If developing an iOS app that runs in the background,
 * please use XMPPStream's enableBackgroundingOnSocket property as opposed to doing it directly on the socket here.
**/
- (void)xmppStream:(XMPPStream *)sender socketDidConnect:(GCDAsyncSocket *)socket;

/**
 * This method is called after the XML stream has been fully opened.
 * More precisely, this method is called after an opening <xml/> and <stream:stream/> tag have been sent and received,
 * and after the stream features have been received, and any required features have been fullfilled.
 * At this point it's safe to begin communication with the server.
**/
- (void)xmppStreamDidConnect:(XMPPStream *)sender;

/**
 * This method is called after authentication has successfully finished.
 * If authentication fails for some reason, the xmppStream:didNotAuthenticate: method will be called instead.
**/
- (void)xmppStreamDidAuthenticate:(XMPPStream *)sender;

/**
 * This method is called if authentication fails.
**/
- (void)xmppStream:(XMPPStream *)sender didNotAuthenticate:(NSXMLElement *)error;

/**
 * This method is called if the XMPP server doesn't allow our resource of choice
 * because it conflicts with an existing resource.
 * 
 * Return an alternative resource or return nil to let the server automatically pick a resource for us.
**/
- (NSString *)xmppStream:(XMPPStream *)sender alternativeResourceForConflictingResource:(NSString *)conflictingResource;

/**
 * These methods are called before their respective XML elements are broadcast as received to the rest of the stack.
 * These methods can be used to modify elements on the fly.
 * (E.g. perform custom decryption so the rest of the stack sees readable text.)
 * 
 * You may also filter incoming elements by returning nil.
 * 
 * When implementing these methods to modify the element, you do not need to copy the given element.
 * You can simply edit the given element, and return it.
 * The reason these methods return an element, instead of void, is to allow filtering.
 * 
 * Concerning thread-safety, delegates implementing the method are invoked one-at-a-time to
 * allow thread-safe modification of the given elements.
 *
 * You should NOT implement these methods unless you have good reason to do so.
 * For general processing and notification of received elements, please use xmppStream:didReceiveX: methods.
 * 
 * @see xmppStream:didReceiveIQ:
 * @see xmppStream:didReceiveMessage:
 * @see xmppStream:didReceivePresence:
**/
- (XMPPIQ *)xmppStream:(XMPPStream *)sender willReceiveIQ:(XMPPIQ *)iq;
- (XMPPMessage *)xmppStream:(XMPPStream *)sender willReceiveMessage:(XMPPMessage *)message;
- (XMPPPresence *)xmppStream:(XMPPStream *)sender willReceivePresence:(XMPPPresence *)presence;

/**
 * This method is called if any of the xmppStream:willReceiveX: methods filter the incoming stanza.
 * 
 * It may be useful for some extensions to know that something was received,
 * even if it was filtered for some reason.
**/
- (void)xmppStreamDidFilterStanza:(XMPPStream *)sender;

/**
 * These methods are called after their respective XML elements are received on the stream.
 * 
 * In the case of an IQ, the delegate method should return YES if it has or will respond to the given IQ.
 * If the IQ is of type 'get' or 'set', and no delegates respond to the IQ,
 * then xmpp stream will automatically send an error response.
 * 
 * Concerning thread-safety, delegates shouldn't modify the given elements.
 * As documented in NSXML / KissXML, elements are read-access thread-safe, but write-access thread-unsafe.
 * If you have need to modify an element for any reason,
 * you should copy the element first, and then modify and use the copy.
**/
- (BOOL)xmppStream:(XMPPStream *)sender didReceiveIQ:(XMPPIQ *)iq;
- (void)xmppStream:(XMPPStream *)sender didReceiveMessage:(XMPPMessage *)message;
- (void)xmppStream:(XMPPStream *)sender didReceivePresence:(XMPPPresence *)presence;

/**
 * This method is called if an XMPP error is received.
 * In other words, a <stream:error/>.
 * 
 * However, this method may also be called for any unrecognized xml stanzas.
 * 
 * Note that standard errors (<iq type='error'/> for example) are delivered normally,
 * via the other didReceive...: methods.
**/
- (void)xmppStream:(XMPPStream *)sender didReceiveError:(NSXMLElement *)error;

/**
 * These methods are called before their respective XML elements are sent over the stream.
 * These methods can be used to modify outgoing elements on the fly.
 * (E.g. add standard information for custom protocols.)
 * 
 * You may also filter outgoing elements by returning nil.
 * 
 * When implementing these methods to modify the element, you do not need to copy the given element.
 * You can simply edit the given element, and return it.
 * The reason these methods return an element, instead of void, is to allow filtering.
 * 
 * Concerning thread-safety, delegates implementing the method are invoked one-at-a-time to
 * allow thread-safe modification of the given elements.
 * 
 * You should NOT implement these methods unless you have good reason to do so.
 * For general processing and notification of sent elements, please use xmppStream:didSendX: methods.
 * 
 * @see xmppStream:didSendIQ:
 * @see xmppStream:didSendMessage:
 * @see xmppStream:didSendPresence:
**/
- (XMPPIQ *)xmppStream:(XMPPStream *)sender willSendIQ:(XMPPIQ *)iq;
- (XMPPMessage *)xmppStream:(XMPPStream *)sender willSendMessage:(XMPPMessage *)message;
- (XMPPPresence *)xmppStream:(XMPPStream *)sender willSendPresence:(XMPPPresence *)presence;

/**
 * These methods are called after their respective XML elements are sent over the stream.
 * These methods may be used to listen for certain events (such as an unavailable presence having been sent),
 * or for general logging purposes. (E.g. a central history logging mechanism).
**/
- (void)xmppStream:(XMPPStream *)sender didSendIQ:(XMPPIQ *)iq;
- (void)xmppStream:(XMPPStream *)sender didSendMessage:(XMPPMessage *)message;
- (void)xmppStream:(XMPPStream *)sender didSendPresence:(XMPPPresence *)presence;

/**
 * These methods are called after failing to send the respective XML elements over the stream.
 * This occurs when the stream gets disconnected before the element can get sent out.
**/
- (void)xmppStream:(XMPPStream *)sender didFailToSendIQ:(XMPPIQ *)iq error:(NSError *)error;
- (void)xmppStream:(XMPPStream *)sender didFailToSendMessage:(XMPPMessage *)message error:(NSError *)error;
- (void)xmppStream:(XMPPStream *)sender didFailToSendPresence:(XMPPPresence *)presence error:(NSError *)error;

/**
 * This method is called if the XMPP Stream's jid changes.
**/
- (void)xmppStreamDidChangeMyJID:(XMPPStream *)xmppStream;

/**
 * This method is called if the disconnect method is called.
 * It may be used to determine if a disconnection was purposeful, or due to an error.
 * 
 * Note: A disconnect may be either "clean" or "dirty".
 * A "clean" disconnect is when the stream sends the closing </stream:stream> stanza before disconnecting.
 * A "dirty" disconnect is when the stream simply closes its TCP socket.
 * In most cases it makes no difference how the disconnect occurs,
 * but there are a few contexts in which the difference has various protocol implications.
 * 
 * @see xmppStreamDidSendClosingStreamStanza
**/
- (void)xmppStreamWasToldToDisconnect:(XMPPStream *)sender;

/**
 * This method is called after the stream has sent the closing </stream:stream> stanza.
 * This signifies a "clean" disconnect.
 * 
 * Note: A disconnect may be either "clean" or "dirty".
 * A "clean" disconnect is when the stream sends the closing </stream:stream> stanza before disconnecting.
 * A "dirty" disconnect is when the stream simply closes its TCP socket.
 * In most cases it makes no difference how the disconnect occurs,
 * but there are a few contexts in which the difference has various protocol implications.
**/
- (void)xmppStreamDidSendClosingStreamStanza:(XMPPStream *)sender;

/**
 * This method is called if the XMPP stream's connect times out.
**/
- (void)xmppStreamConnectDidTimeout:(XMPPStream *)sender;

/**
 * This method is called after the stream is closed.
 * 
 * The given error parameter will be non-nil if the error was due to something outside the general xmpp realm.
 * Some examples:
 * - The TCP socket was unexpectedly disconnected.
 * - The SRV resolution of the domain failed.
 * - Error parsing xml sent from server.
 * 
 * @see xmppStreamConnectDidTimeout:
**/
- (void)xmppStreamDidDisconnect:(XMPPStream *)sender withError:(NSError *)error;


/**
 * These methods are called as xmpp modules are registered and unregistered with the stream.
 * This generally corresponds to xmpp modules being initailzed and deallocated.
 * 
 * The methods may be useful, for example, if a more precise auto delegation mechanism is needed
 * than what is available with the autoAddDelegate:toModulesOfClass: method.
**/
- (void)xmppStream:(XMPPStream *)sender didRegisterModule:(id)module;
- (void)xmppStream:(XMPPStream *)sender willUnregisterModule:(id)module;

/**
 * Custom elements are Non-XMPP elements.
 * In other words, not <iq>, <message> or <presence> elements.
 * 
 * Typically these kinds of elements are not allowed by the XMPP server.
 * But some custom implementations may use them.
 * The standard example is XEP-0198, which uses <r> & <a> elements.
 * 
 * If you're using custom elements, you must register the custom element name(s).
 * Otherwise the xmppStream will treat non-XMPP elements as errors (xmppStream:didReceiveError:).
 * 
 * @see registerCustomElementNames (in XMPPInternal.h)
**/
- (void)xmppStream:(XMPPStream *)sender didSendCustomElement:(NSXMLElement *)element;
- (void)xmppStream:(XMPPStream *)sender didReceiveCustomElement:(NSXMLElement *)element;

@end
