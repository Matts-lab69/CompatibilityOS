# binfmt_misc integration

Future terminal association for `MZ` files, as supported by the [Linux binfmt_misc documentation](https://docs.kernel.org/admin-guide/binfmt-misc.html). `MZ` is a dispatch hint, not PE validation. MSI is not handled by this rule; Wine's [MSI dispatch](https://github.com/wine-mirror/wine/blob/7298a818b79446bae153b8d5aa27235ab3703bf9/programs/msiexec/msiexec.c#L1108-L1111) is a separate path. Registration is not implemented. OpenRC persistence and handling of existing competing rules remain unverified.
