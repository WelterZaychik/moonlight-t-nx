# Embedded Tailscale engine provenance

Source: https://github.com/UnderwaterOverground/tailscale-nx
Revision: `70fe63f1be50bf41e12fd5f26dd45a867ad95ac5` (1.4.1)

Included: Rust workspace and lockfile, native runtime/VSock/driver, vendored
tailscale-rs, and Project Nayuki's QR generator. Atmosphère, libstratosphere,
the system module, MITM service and Tesla/Ultrahand overlay are not linked.
Original LICENSE and THIRD_PARTY_NOTICES.md are retained. Rust workspace
package metadata declares BSD-3-Clause; the upstream repository also supplies
a GPL-2.0 top-level license. These notices are retained without relicensing
upstream files; review the upstream component licenses before redistribution.

Local changes:

- Add Runtime::StopAndJoin() for deterministic engine replacement while old
  application socket descriptors still retain their Session.
- Check network-driver allocation failure in Runtime::Start().
- Guard the host-only Taildrop filesystem adapter when compiling the shared
  runtime for Switch. This frontend does not enable Taildrop.

The application adapter under app/tailscale is separate from the vendored code.
The integration design was also informed by
https://github.com/davidkalil10/super-moonlight-nx2, whose NRO and SDK target
Switch 2. Its platform adapter was inspected, rather than copied into this app.
