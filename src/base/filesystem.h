// https://github.com/felix-u 2026-04-15
// Public domain. NO WARRANTY - use at your own risk.

#if !defined(FS_H)
#define FS_H


#if defined(__wasm__) || defined(__wasm32__)
    #define FS_OS_WASM
#elif defined(_WIN32)
    #define FS_OS_WINDOWS
#else
    // NOTE(felix): it may be good to distinguish between non-Windows OSes
    #define FS_OS_POSIX
#endif

#if !defined(FS_NO_SYSTEM_INCLUDE) && defined(FS_OS_POSIX)
    #include <stdio.h>
#endif

typedef struct {
    #if defined(FS_OS_WINDOWS)
        void *handle;
    #elif defined(FS_OS_POSIX)
        FILE *handle;
    #else
        int _stub;
    #endif
} fs_File;

typedef struct {
    const char *folder_path;

    #if defined(FS_OS_WINDOWS)
        void *handle;
        WIN32_FIND_DATAA data;
        _Bool is_first_file;

        char path[260 + 1];
        char name[260 + 1];
    #elif defined(FS_OS_POSIX)
        FILE *handle;
        char path[4096];
        char name[4096];
    #else
        int _stub;
    #endif
} fs_Folder_Iterator;

typedef enum {
    fs_File_Flag_READ  = 1 << 0,
    fs_File_Flag_WRITE = 1 << 1,
} fs_File_Flags;

#if !defined(FS_FUNCTION)
    #define FS_FUNCTION
#endif

FS_FUNCTION _Bool              fs_close(fs_File file);
FS_FUNCTION void               fs_absolute_path(const char *path, char *buffer, unsigned long long buffer_size);
FS_FUNCTION _Bool              fs_exists(const char *path);
FS_FUNCTION fs_File            fs_file_open_and_get_size(const char *path, fs_File_Flags flags, unsigned long long *size);
FS_FUNCTION unsigned long long fs_file_size(const char *path);
FS_FUNCTION _Bool              fs_file_write(fs_File file, const void *bytes, unsigned long long size);
FS_FUNCTION _Bool              fs_folder_iterate(fs_Folder_Iterator *it);
FS_FUNCTION fs_Folder_Iterator fs_folder_iterator(const char *relative_path);
FS_FUNCTION void               fs_folder_iterator_end(fs_Folder_Iterator *it);
FS_FUNCTION _Bool              fs_make_folder(const char *relative_path);
FS_FUNCTION fs_File            fs_open(const char *path, fs_File_Flags flags);
FS_FUNCTION unsigned long long fs_read_entire_file(fs_File file, char *buffer, unsigned long long size);
FS_FUNCTION _Bool              fs_remove_file(const char *relative_path);


#endif // FS_H


#if defined(FS_IMPLEMENTATION)


#if !defined(FS_ASSERT)
    #include <assert.h>
    #define FS_ASSERT assert
#endif

#if !defined(FS_NO_SYSTEM_INCLUDE)
    #if defined(FS_OS_WINDOWS)
        #if defined(_MSC_VER)
            #pragma comment(lib, "Kernel32.lib")
        #endif
        #include <windows.h>
    #elif defined(FS_OS_POSIX)
        #include <stdio.h>
        #include <stdlib.h>
    #endif
#endif

#define FS_OS_WINDOWS_INVALID_HANDLE_VALUE ((void *)(long long)-1)

#if defined(FS_OS_WINDOWS)
    #define FS_PATH_SEPARATOR '\\'
#else
    #define FS_PATH_SEPARATOR '/'
#endif

FS_FUNCTION _Bool fs_close(fs_File file) {
    _Bool ok = 0;
    if (file.handle == 0) return ok;

    #if defined(FS_OS_WINDOWS)
    {
        ok = !!CloseHandle(file.handle);
    }
    #elif defined(FS_OS_POSIX)
    {
        fclose(file.handle);
        ok = 1;
    }
    #else
        #error "UNSUPPORTED OS"
    #endif

    return ok;
}

FS_FUNCTION void fs_absolute_path(const char *path, char *buffer, unsigned long long buffer_size) {
    #if defined(FS_OS_WINDOWS)
    {
        fs_File temp = fs_open(path, 0);
        if (temp.handle != 0) {
            FS_ASSERT(buffer_size <= 0xffffffff);
            unsigned long size = (unsigned long)buffer_size;
            GetFinalPathNameByHandleA(temp.handle, buffer, size, 0);
        }
        fs_close(temp);
    }
    #elif defined(FS_OS_POSIX)
    {
        FS_ASSERT(buffer_size >= 4096);
        realpath(path, buffer);
    }
    #else
        #error "UNSUPPORTED OS"
    #endif
}

FS_FUNCTION _Bool fs_exists(const char *path) {
    _Bool exists = 0;

    #if defined(FS_OS_WINDOWS)
    {
        unsigned long attributes = GetFileAttributesA(path);
        exists = attributes != (unsigned long)(-1);
    }
    #elif defined(FS_OS_POSIX)
    {
        struct stat status = {0};
        exists = stat(path, &status) == 0;
    }
    #else
        #error "UNSUPPORTED OS"
    #endif

    return exists;
}

FS_FUNCTION fs_File fs_file_open_and_get_size(const char *path, fs_File_Flags flags, unsigned long long *size) {
    fs_File file = fs_open(path, flags);
    if (file.handle != 0) {
        #if defined(FS_OS_WINDOWS)
        {
            int ok = GetFileSizeEx(file.handle, (LARGE_INTEGER *)size);
            if (!ok) *size = 0;
        }
        #elif defined(FS_OS_POSIX)
        {
            *size = fs_file_size(path);
        }
        #else
            #error "UNSUPPORTED OS"
        #endif
    }
    return file;
}

FS_FUNCTION unsigned long long fs_file_size(const char *path) {
    unsigned long long result = 0;

    #if defined(FS_OS_WINDOWS)
    {
        fs_File temp = fs_file_open_and_get_size(path, 0, &result);
        fs_close(temp);
    }
    #elif defined(FS_OS_POSIX)
    {
        struct stat status = {0};
        if (lstat(path, &status) == 0) {
            result = (unsigned long long)status.st_size;
        }
    }
    #else
        #error "UNSUPPORTED OS"
    #endif

    return result;
}

FS_FUNCTION _Bool fs_file_write(fs_File file, const void *bytes, unsigned long long size) {
    _Bool ok = 0;
    if (file.handle == 0) return ok;

    #if defined(FS_OS_WINDOWS)
    {
        FS_ASSERT(size <= 0xffffffff);
        ok = !!WriteFile(file.handle, bytes, (unsigned long)size, 0, 0);
    }
    #elif defined(FS_OS_POSIX)
    {
        ok = 1;
        for (unsigned long long total_written = 0; total_written < size;) {
            unsigned long long written = fwrite((char *)bytes + total_written, 1, size - total_written, file.handle);
            if (written == 0) {
                ok = 0;
                break;
            }
            total_written += written;
        }
    }
    #else
        #error "UNSUPPORTED OS"
    #endif

    return ok;
}

static void fs__folder_iterator_fill_info(fs_Folder_Iterator *it) {
    if (it->handle != 0) {
        #if defined(FS_OS_WINDOWS)
        {
            int i = 0;
            while (i < (int)(sizeof it->data.cFileName) && it->data.cFileName[i] != 0) {
                it->name[i] = it->data.cFileName[i];
                i += 1;
            }
            it->name[i] = 0;
        }
        #else
        {
            FS_ASSERT(0 && "unimplemented for this OS");
        }
        #endif

        int i = 0;
        while (it->folder_path[i] != 0) {
            it->path[i] = it->folder_path[i];
            i += 1;
        }

        it->path[i++] = FS_PATH_SEPARATOR;

        for (int j = 0; it->name[j] != 0; j += 1, i += 1) {
            it->path[i] = it->name[j];
        }

        FS_ASSERT(i < (int)(sizeof it->path));
        it->path[i++] = 0;
    }
}

FS_FUNCTION _Bool fs_folder_iterate(fs_Folder_Iterator *it) {
    _Bool keep_going = 0;

    if (it->handle != 0) {
        #if defined(FS_OS_WINDOWS)
        {
            if (it->is_first_file) {
                it->is_first_file = 0;
                keep_going = 1;
            } else {
                keep_going = !!FindNextFileA(it->handle, &it->data);
            }
        }
        #else
        {
            FS_ASSERT(0 && "unimplemented for this OS");
        }
        #endif
    }

    fs__folder_iterator_fill_info(it);
    return keep_going;
}

FS_FUNCTION fs_Folder_Iterator fs_folder_iterator(const char *relative_path) {
    fs_Folder_Iterator it = { .folder_path = relative_path };

    #if defined(FS_OS_WINDOWS)
    {
        char glob[261] = {0};
        int i = 0;
        while (it.folder_path[i] != 0 && i < (int)(sizeof glob)) {
            glob[i] = it.folder_path[i];
            i += 1;
        }

        FS_ASSERT(i + 2 < (int)(sizeof glob));
        glob[i++] = '\\';
        glob[i++] = '*';

        it.handle = FindFirstFileA(glob, &it.data);
        if (it.handle == FS_OS_WINDOWS_INVALID_HANDLE_VALUE) it.handle = 0;
        else it.is_first_file = 1;
    }
    #else
    {
        FS_ASSERT(0 && "unimplemented for this OS");
    }
    #endif

    fs__folder_iterator_fill_info(&it);
    return it;
}

FS_FUNCTION void fs_folder_iterator_end(fs_Folder_Iterator *it) {
    if (it->handle == 0) return;

    #if defined(FS_OS_WINDOWS)
    {
        _Bool ok = !!FindClose(it->handle);
        (void)ok;
    }
    #else
    {
        (void)it;
        FS_ASSERT(0 && "unimplemented for this OS");
    }
    #endif
}

FS_FUNCTION _Bool fs_make_folder(const char *relative_path) {
    _Bool ok = 0;

    #if defined(FS_OS_WINDOWS)
    {
        ok = !!CreateDirectoryA(relative_path, 0);
    }
    #elif defined(FS_OS_POSIX)
    {
        mode_t mode = 0755;
        int result = mkdir(relative_path, mode);
        ok = result == 0;
    }
    #else
        #error "UNSUPPORTED OS"
    #endif

    return ok;
}

FS_FUNCTION fs_File fs_open(const char *path, fs_File_Flags flags) {
    fs_File result = {0};

    flags += !flags * fs_File_Flag_READ;

    _Bool read = !!(flags & fs_File_Flag_READ);
    _Bool write = !!(flags & fs_File_Flag_WRITE);
    FS_ASSERT(read || write);
    FS_ASSERT(read ^ write); // TODO(felix): correctly handle read+write

    #if defined(FS_OS_WINDOWS)
    {
        // https://learn.microsoft.com/en-us/windows/win32/secauthz/generic-access-rights
        enum { generic_write = 0x40000000 };
        unsigned long generic_read = 0x80000000; // (not representable in enum by int)

        // https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilea
        enum { create_always = 2, create_new = 1, open_always = 4, open_existing = 3, truncate_existing = 5 };
        enum { file_share_delete = 4, file_share_read = 1, file_share_write = 2 };
        enum { file_attribute_normal = 128 };
        enum { file_flag_backup_semantics = 0x02000000 };

        unsigned long desired_access = (read * generic_read) | (write * generic_write);
        unsigned long creation_disposition = (read * open_existing) | (write * create_always);
        // file_flag_backup_semantics required to open directories
        unsigned long flags_and_attributes = (unsigned long)file_attribute_normal | (unsigned long)file_flag_backup_semantics;

        void *handle = CreateFileA(path, desired_access, file_share_read | file_share_delete, 0, creation_disposition, flags_and_attributes, 0);

        if (handle != FS_OS_WINDOWS_INVALID_HANDLE_VALUE) result.handle = handle;
    }
    #elif defined(FS_OS_POSIX)
    {
        const char *mode = 0;
        if (read) mode = "rb";
        if (write) mode = "wb";
        FILE *handle = fopen(path, mode);
        result.handle = handle;
    }
    #else
        #error "UNSUPPORTED OS"
    #endif

    return result;
}

FS_FUNCTION unsigned long long fs_read_entire_file(fs_File file, char *buffer, unsigned long long size) {
    unsigned long long read = 0;
    if (file.handle == 0) return read;
    FS_ASSERT(buffer != 0);

    #if defined(FS_OS_WINDOWS)
    {
        unsigned long bytes_read = 0;
        (void)bytes_read;

        int ok = !!ReadFile(file.handle, buffer, (unsigned)size, &bytes_read, 0);
        if (ok) read = (unsigned long long)bytes_read;
    }
    #elif defined(FS_OS_POSIX)
    {
        rewind(file.handle);
        unsigned long long bytes_read = fread(buffer, 1, size, file.handle);
        _Bool ok = bytes_read == size;
        if (ok) read = bytes_read;
    }
    #else
        #error "UNSUPPORTED OS"
    #endif

    return read;
}

FS_FUNCTION _Bool fs_remove_file(const char *relative_path) {
    _Bool ok = 0;

    #if defined(FS_OS_WINDOWS)
    {
        ok = !!DeleteFileA(relative_path);
    }
    #elif defined(FS_OS_POSIX)
    {
        int result = remove(relative_path);
        ok = result == 0;
    }
    #else
        #error "UNSUPPORTED OS"
    #endif

    return ok;
}

#endif // FS_IMPLEMENTATION
