#ifndef GIT_VERSION_H
#define GIT_VERSION_H

#ifdef __cplusplus
namespace Git_version {
extern const char* const branch;
extern const char* const sha1;
extern const char* const shortSha1;
extern const char* const tags;
extern const char* const config;
extern const bool        dirty;
}
#else
extern const char* const Gv_branch;
extern const char* const Gv_sha1;
extern const char* const Gv_shortSha1;
extern const char* const Gv_tags;
extern const char* const Gv_config;
extern const bool        Gv_dirty;
#endif

#endif // GIT_VERSION_H

