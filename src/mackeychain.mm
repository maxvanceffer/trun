#include "mackeychain.h"

#include <Security/Security.h>

namespace {

CFStringRef toCFString(const QString &s)
{
    return CFStringCreateWithCharacters(kCFAllocatorDefault,
                                        reinterpret_cast<const UniChar *>(s.utf16()),
                                        s.length());
}

CFMutableDictionaryRef baseQuery(const QString &service, const QString &account)
{
    CFMutableDictionaryRef q = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(q, kSecClass, kSecClassGenericPassword);
    CFStringRef svc = toCFString(service);
    CFStringRef acc = toCFString(account);
    CFDictionarySetValue(q, kSecAttrService, svc);
    CFDictionarySetValue(q, kSecAttrAccount, acc);
    CFRelease(svc);
    CFRelease(acc);
    return q;
}

} // namespace

bool macKeychainSetPassword(const QString &service, const QString &account, const QString &password)
{
    CFMutableDictionaryRef query = baseQuery(service, account);
    const QByteArray secret = password.toUtf8();
    CFDataRef data = CFDataCreate(kCFAllocatorDefault,
                                  reinterpret_cast<const UInt8 *>(secret.constData()),
                                  secret.size());

    // Item exists -> update its secret; otherwise add a new one.
    OSStatus status = SecItemCopyMatching(query, nullptr);
    if (status == errSecSuccess) {
        CFMutableDictionaryRef update = CFDictionaryCreateMutable(
            kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFDictionarySetValue(update, kSecValueData, data);
        status = SecItemUpdate(query, update);
        CFRelease(update);
    } else {
        CFDictionarySetValue(query, kSecValueData, data);
        CFDictionarySetValue(query, kSecAttrAccessible, kSecAttrAccessibleAfterFirstUnlock);
        status = SecItemAdd(query, nullptr);
    }

    CFRelease(data);
    CFRelease(query);
    return status == errSecSuccess;
}

bool macKeychainFindPassword(const QString &service, const QString &account, QString &password)
{
    CFMutableDictionaryRef query = baseQuery(service, account);
    CFDictionarySetValue(query, kSecReturnData, kCFBooleanTrue);
    CFDictionarySetValue(query, kSecMatchLimit, kSecMatchLimitOne);

    CFTypeRef result = nullptr;
    const OSStatus status = SecItemCopyMatching(query, &result);
    CFRelease(query);
    if (status != errSecSuccess || !result)
        return false;

    CFDataRef data = static_cast<CFDataRef>(result);
    password = QString::fromUtf8(reinterpret_cast<const char *>(CFDataGetBytePtr(data)),
                                 CFDataGetLength(data));
    CFRelease(result);
    return true;
}

bool macKeychainDeletePassword(const QString &service, const QString &account)
{
    CFMutableDictionaryRef query = baseQuery(service, account);
    const OSStatus status = SecItemDelete(query);
    CFRelease(query);
    return status == errSecSuccess || status == errSecItemNotFound;
}
