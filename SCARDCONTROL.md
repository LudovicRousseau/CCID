# List of SCardControl() commands supported by the CCID driver

PC/SC provides the `SCardControl()` function to send commands to the
driver and/or reader. Here is the list of supported commands by this
CCID driver.


## PC/SC function prototype

See also [SCardControl()](https://pcsclite.apdu.fr/api/group__API.html#gac3454d4657110fd7f753b2d3d8f4e32f) in the pcsclite API documentation.

```C
LONG SCardControl(
    SCARDHANDLE hCard,
    DWORD       dwControlCode,
    LPCVOID     pbSendBuffer,
    DWORD       cbSendLength,
    LPVOID      pbRecvBuffer,
    DWORD       cbRecvLength,
    LPDWORD     lpBytesReturned
)
```

Parameters:

* [in]     `hCard`            Connection made from `SCardConnect()`.
* [in]     `dwControlCode`    Control code for the operation.
* [in]     `pbSendBuffer`     Command to send to the reader.
* [in]     `cbSendLength`     Length of the command.
* [out]    `pbRecvBuffer`     Response from the reader.
* [in]     `cbRecvLength`     Length of the response buffer.
* [out]    `lpBytesReturned`  Length of the response.

If the `dwControlCode` is not supported the application receives the error
`SCARD_E_UNSUPPORTED_FEATURE` or `SCARD_E_NOT_TRANSACTED` as a general error
code.


## supported ControlCode values

* `IOCTL_SMARTCARD_VENDOR_IFD_EXCHANGE`

    defined as `SCARD_CTL_CODE(1)`

    The `pbSendBuffer[]` buffer is sent as a `PC_to_RDR_Escape` CCID command

    For security possible problems this command in possible in the
    following cases only:
    - the reader is a Gemalto (ex Gemplus) reader and the command is:
      - get firmware version
      - switch interface on a ProxDU
    - the `ifdDriverOptions` (in the `Info.plist` file) has the bit
      `DRIVER_OPTION_CCID_EXCHANGE_AUTHORIZED` set

* `CM_IOCTL_GET_FEATURE_REQUEST`

    defined as `SCARD_CTL_CODE(3400)`

    Implements the PC/SC v2.02.08 Part 10 IOCTL mechanism

* `IOCTL_FEATURE_VERIFY_PIN_DIRECT`
* `IOCTL_FEATURE_MODIFY_PIN_DIRECT`
* `IOCTL_FEATURE_IFD_PIN_PROPERTIES`
* `IOCTL_FEATURE_MCT_READERDIRECT`
* `IOCTL_FEATURE_GET_TLV_PROPERTIES`

    See PC/SC v2.02.08 Part 10

