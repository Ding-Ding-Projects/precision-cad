# Disposable release-verification environment

This route creates evidence for a native Squirrel.Windows installer inside a task-owned Hyper-V guest. It never changes the host user's install profile, redirects `LOCALAPPDATA`, weakens host security settings, stops an existing virtual machine, or searches disks for operating-system media.

## Current host result

On 2026-09-07, the Hyper-V module exposed `Get-VHD`, `New-VM`, and `Checkpoint-VM`, but read-only `Get-VM` returned an authorization-policy refusal and the feature-state query required elevation. Therefore no guest was provisioned and no installer runtime result is claimed. The repeatable preflight records that exact condition as a blocked receipt:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/release-environment/Test-ReleaseEnvironmentPreflight.ps1 -VmName PrecisionCAD-ReleaseVerification -OutputPath artifacts/release-environment/preflight.json
```

The command exits `2` for a blocked preflight. An approved base image is always passed by explicit path. The preflight hashes that one file but does not discover images elsewhere.

## Authorized provisioning route

Provisioning requires an approved and licensed base VHDX, an explicit existing virtual switch, an unused task-owned VM name and directory, and a user-provided licence acknowledgement. It refuses an existing VM or VM path. It does not select, stop, alter, or delete any existing VM or network.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/release-environment/New-ReleaseEnvironmentVm.ps1 `
  -BaseImagePath D:\approved-media\Windows-guest.vhdx `
  -VmName PrecisionCAD-ReleaseVerification `
  -VmPath D:\PrecisionCAD-ReleaseVerification `
  -SwitchName 'Approved isolated switch' `
  -AcceptBaseImageLicense `
  -PreflightPath artifacts/release-environment/preflight.json
```

The base image is an external licensed input. This repository neither downloads an evaluation operating system nor includes credentials. Guest access must use a caller-provided `PSCredential` supplied through an approved protected route. The host runner never guesses, persists, prints, or accepts a password in chat.

## Guest execution and receipts

After the guest has been prepared by the approved provisioning route, use its receipt together with a protected credential object. The host revalidates the created VM ID, Generation 2, VM path, VHD hash, switch identity, processor count, and a running state before it copies any candidate files. It creates a fresh GUID-named guest staging directory, never reuses `C:\ReleaseVerification`, and copies the receipt back before declaring the blocked result.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/release-environment/Invoke-GuestReleaseVerification.ps1 `
  -VmName PrecisionCAD-ReleaseVerification `
  -Credential $approvedCredential `
  -ProvisioningReceiptPath artifacts/release-environment/provisioning-receipt.json `
  -GuestScriptPath scripts/release-environment/Invoke-GuestVerification.ps1 `
  -GuestArtifactDirectory artifacts/native/squirrel-windows/<candidate> `
  -GuestReceiptPath artifacts/release-environment/runtime-receipt.json
```

The guest route is intentionally receipt-first. It records the setup hash, isolation facts, attempted installation and launch states, and a reserved deterministic prior/candidate updater feed section.

The updater section remains unverified until the native updater exists and a credential-free deterministic HTTPS feed serves the ordered prior and candidate artifacts. A full future receipt must contain observed `available`, `downloading`, and `ready-to-restart` states, package-hash validation, a release-note link, the unsigned warning, and explicit restart or later actions.

## Process-observation Chut

The guest does not falsify a successful installation when process observation is incomplete. The known `Win32_ProcessStartTrace` `AccessDenied` condition produces `mode: blocked`, `exhaustive: false`, and a blocked verdict. Sampling with `Get-Process` is explicitly incomplete because short-lived processes may escape it. The receipt validator accepts that as a truthful blocked outcome and rejects it when `-RequireComplete` is specified:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/release-environment/Test-GuestReleaseReceipt.ps1 -ReceiptPath artifacts/release-environment/runtime-receipt.json -RequireComplete
```

When the observation Chut is available, the final verification must use the cheap Lowlevel headless route inside the guest: create a named off-screen desktop, launch the installed executable directly, resolve the non-zero application window by exact title and class, drive the updater flow, and retain the guest-only receipt and captures. No visible desktop or host profile is used.

## Focused validation

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tests/release-environment/test-release-environment.ps1
```

This validation proves the infrastructure refuses unsafe provisioning and does not upgrade the known observation blocker into a runtime success claim. It is not installer, launch, update, or release evidence.
