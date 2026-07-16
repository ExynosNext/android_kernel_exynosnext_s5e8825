# ExynosNext Kernel — Security Policy

## Supported Versions

| Version | Supported |
|---------|-----------|
| Latest  | Yes       |
| < Latest | No       |

## Reporting a Vulnerability

If you discover a security vulnerability in ExynosNext Kernel, please report it responsibly:

**Do NOT open a public issue for security vulnerabilities.**

Instead, please email: [security@exynosnext.dev](mailto:security@exynosnext.dev)

Include:
- Description of the vulnerability
- Steps to reproduce
- Potential impact
- Suggested fix (if any)

## Response Timeline

- **Acknowledgment**: Within 48 hours
- **Initial assessment**: Within 1 week
- **Fix timeline**: Depends on severity (critical: 24-48h, high: 1 week, medium: 2 weeks)

## Scope

This security policy applies to:
- The ExynosNext Kernel source code
- Build scripts and CI pipeline
- Docker build environment

It does NOT apply to:
- Samsung's original kernel source
- Android Common Kernel (ACK)
- Third-party vendor modules

## Kernel Security Features

ExynosNext Kernel includes the following security hardening options:

```
CONFIG_ARM64_PTR_AUTH=y
CONFIG_ARM64_BTI=y
CONFIG_ARM64_MTE=y
CONFIG_ARM64_E0PD=y
CONFIG_STRICT_DEVMEM=y
CONFIG_RANDOMIZE_BASE=y
CONFIG_STACKPROTECTOR_STRONG=y
CONFIG_FORTIFY_SOURCE=y
CONFIG_INIT_ON_ALLOC_DEFAULT_ON=y
CONFIG_INIT_ON_FREE_DEFAULT_ON=y
CONFIG_HARDENED_USERCOPY=y
CONFIG_CFI_CLANG=y
CONFIG_SHADOW_CALL_STACK=y
CONFIG_PAGE_TABLE_ISOLATION=y
CONFIG_SECURITY_YAMA=y
```

## Disclosure Policy

We follow coordinated disclosure. Please allow reasonable time for a fix before public disclosure.
