[![Build Status](https://travis-ci.com/KONNEKTING/KonnektingDeviceLibrary.svg?branch=develop_beta4a)](https://travis-ci.com/KONNEKTING/KonnektingDeviceLibrary)

# KONNEKTING Device Library
This is a library for Arduino boards, that allow creating own DIY KNX devices, programmable via KNX bus.... 

The base is a fork of Franck Marini's "KnxDevice" with an important difference: The resulting KNX-enabled Arduino device can be programmed via KNX bus (no, not via ETS). The programming mechanism uses an [own, extensible 14-byte-group-address protocol](https://github.com/KONNEKTING/KonnektingDocumentation/blob/master/protocol_general.md). A java-powered software tool is available to programm your "KONNEKTING device" with parameters and group-addresses.

We're more and more rewriting the code, to create our own, independant project.

For more information, please follow the link to our [documentation project](https://github.com/KONNEKTING/KonnektingDocumentation/blob/master/README.md).

# !!! WARNING !!!

*If you aren't an expert, stick with the pre-build/packaged releases you can download here: https://github.com/KONNEKTING/KonnektingDeviceLibrary/releases and DON'T pull code from any branch.
Otherwise you might face weird problems and bug-reporting get's worse, because you aren't able to name the version that failed.*

