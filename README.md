
## XMPPFramework

An XMPP Framework in Objective-C for the Mac and iOS development community.

### Install

The minimum deployment target is iOS 8.0 / macOS 10.9.

#### CocoaPods

The easiest way to install XMPPFramework is using CocoaPods. Remember to add to the top of your `Podfile` the `use_frameworks!` line (even if you are not using swift):

This will install the whole framework with all the available extensions:

```ruby
use_frameworks!
pod 'XMPPFramework', :git => 'https://github.com/pankajsoni19/XMPPFramework.git', :tag => '3.7.1'
```

After `pod install` open the `.xcworkspace` and import:

```
import XMPPFramework      // swift
@import XMPPFramework;   //objective-c
```

### Wiki:
For more info please go to original repo.

- [Original Repo](https://github.com/robbiehanson/XMPPFramework)

