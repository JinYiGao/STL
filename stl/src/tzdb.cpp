// Copyright (c) Microsoft Corporation.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <atomic>
#include <icu.h>
#include <internal_shared.h>
#include <memory>
#include <string_view>
#include <xfilesystem_abi.h>
#include <xtzdb.h>

#include <Windows.h>

#pragma comment(lib, "Advapi32")

namespace {
    enum class _Icu_api_level : unsigned long {
        _Not_set,
        _Detecting,
        _Has_failed,
        _Has_icu_addresses,
    };

    struct _Icu_functions_table {
        _STD atomic<decltype(&::ucal_getCanonicalTimeZoneID)> _Pfn_ucal_getCanonicalTimeZoneID{nullptr};
        _STD atomic<decltype(&::ucal_getDefaultTimeZone)> _Pfn_ucal_getDefaultTimeZone{nullptr};
        _STD atomic<decltype(&::ucal_getTZDataVersion)> _Pfn_ucal_getTZDataVersion{nullptr};
        _STD atomic<decltype(&::ucal_openTimeZoneIDEnumeration)> _Pfn_ucal_openTimeZoneIDEnumeration{nullptr};
        _STD atomic<decltype(&::uenum_close)> _Pfn_uenum_close{nullptr};
        _STD atomic<decltype(&::uenum_count)> _Pfn_uenum_count{nullptr};
        _STD atomic<decltype(&::uenum_unext)> _Pfn_uenum_unext{nullptr};
        _STD atomic<_Icu_api_level> _Api_level{_Icu_api_level::_Not_set};
    };

    _Icu_functions_table _Icu_functions;

    template <class _Ty>
    void _Load_address(
        const HMODULE _Module, _STD atomic<_Ty>& _Stored_Pfn, LPCSTR _Fn_name, DWORD& _Last_error) noexcept {
        const auto _Pfn = reinterpret_cast<_Ty>(GetProcAddress(_Module, _Fn_name));
        if (_Pfn != nullptr) {
            _Stored_Pfn.store(_Pfn, _STD memory_order_relaxed);
        } else {
            _Last_error = GetLastError();
        }
    }

    _NODISCARD _Icu_api_level _Init_icu_functions(_Icu_api_level _Level) noexcept {
        while (!_Icu_functions._Api_level.compare_exchange_weak(
            _Level, _Icu_api_level::_Detecting, _STD memory_order_acq_rel)) {
            if (_Level > _Icu_api_level::_Detecting) {
                return _Level;
            }
        }

        _Level = _Icu_api_level::_Has_failed;

        const HMODULE _Icu_module = LoadLibraryExW(L"icu.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (_Icu_module != nullptr) {
            // collect at least one error if any GetProcAddress call fails
            DWORD _Last_error{ERROR_SUCCESS};
            _Load_address(_Icu_module, _Icu_functions._Pfn_ucal_getCanonicalTimeZoneID, "ucal_getCanonicalTimeZoneID",
                _Last_error);
            _Load_address(
                _Icu_module, _Icu_functions._Pfn_ucal_getDefaultTimeZone, "ucal_getDefaultTimeZone", _Last_error);
            _Load_address(_Icu_module, _Icu_functions._Pfn_ucal_openTimeZoneIDEnumeration,
                "ucal_openTimeZoneIDEnumeration", _Last_error);
            _Load_address(_Icu_module, _Icu_functions._Pfn_ucal_getTZDataVersion, "ucal_getTZDataVersion", _Last_error);
            _Load_address(_Icu_module, _Icu_functions._Pfn_uenum_close, "uenum_close", _Last_error);
            _Load_address(_Icu_module, _Icu_functions._Pfn_uenum_count, "uenum_count", _Last_error);
            _Load_address(_Icu_module, _Icu_functions._Pfn_uenum_unext, "uenum_unext", _Last_error);
            if (_Last_error == ERROR_SUCCESS) {
                _Level = _Icu_api_level::_Has_icu_addresses;
            } else {
                // reset last error code for thread in case a later GetProcAddress resets it
                SetLastError(_Last_error);
            }
        }

        _Icu_functions._Api_level.store(_Level, _STD memory_order_release);
        return _Level;
    }

    _NODISCARD _Icu_api_level _Acquire_icu_functions() noexcept {
        auto _Level = _Icu_functions._Api_level.load(_STD memory_order_acquire);
        if (_Level <= _Icu_api_level::_Detecting) {
            _Level = _Init_icu_functions(_Level);
        }

        return _Level;
    }

    _NODISCARD int32_t __icu_ucal_getCanonicalTimeZoneID(const UChar* id, int32_t len, UChar* result,
        int32_t resultCapacity, UBool* isSystemID, UErrorCode* status) noexcept {
        const auto _Fun = _Icu_functions._Pfn_ucal_getCanonicalTimeZoneID.load(_STD memory_order_relaxed);
        return _Fun(id, len, result, resultCapacity, isSystemID, status);
    }

    _NODISCARD int32_t __icu_ucal_getDefaultTimeZone(UChar* result, int32_t resultCapacity, UErrorCode* ec) noexcept {
        const auto _Fun = _Icu_functions._Pfn_ucal_getDefaultTimeZone.load(_STD memory_order_relaxed);
        return _Fun(result, resultCapacity, ec);
    }

    _NODISCARD UEnumeration* __icu_ucal_openTimeZoneIDEnumeration(
        USystemTimeZoneType zoneType, const char* region, const int32_t* rawOffset, UErrorCode* ec) noexcept {
        const auto _Fun = _Icu_functions._Pfn_ucal_openTimeZoneIDEnumeration.load(_STD memory_order_relaxed);
        return _Fun(zoneType, region, rawOffset, ec);
    }

    _NODISCARD const char* __icu_ucal_getTZDataVersion(UErrorCode* status) noexcept {
        const auto _Fun = _Icu_functions._Pfn_ucal_getTZDataVersion.load(_STD memory_order_relaxed);
        return _Fun(status);
    }

    _NODISCARD void __icu_uenum_close(UEnumeration* en) noexcept {
        const auto _Fun = _Icu_functions._Pfn_uenum_close.load(_STD memory_order_relaxed);
        return _Fun(en);
    }

    _NODISCARD int32_t __icu_uenum_count(UEnumeration* en, UErrorCode* ec) noexcept {
        const auto _Fun = _Icu_functions._Pfn_uenum_count.load(_STD memory_order_relaxed);
        return _Fun(en, ec);
    }

    _NODISCARD const UChar* __icu_uenum_unext(UEnumeration* en, int32_t* resultLength, UErrorCode* status) noexcept {
        const auto _Fun = _Icu_functions._Pfn_uenum_unext.load(_STD memory_order_relaxed);
        return _Fun(en, resultLength, status);
    }

    _NODISCARD const char* _Allocate_wide_to_narrow(
        const char16_t* const _Input, const int _Input_len, __std_tzdb_error& _Err) noexcept {
        const auto _Code_page      = __std_fs_code_page();
        const auto _Input_as_wchar = reinterpret_cast<const wchar_t*>(_Input);
        const auto _Count_result = __std_fs_convert_wide_to_narrow(_Code_page, _Input_as_wchar, _Input_len, nullptr, 0);
        if (_Count_result._Err != __std_win_error::_Success) {
            _Err = __std_tzdb_error::_Win_error;
            return nullptr;
        }

        _STD unique_ptr<char[]> _Data{new (_STD nothrow) char[_Count_result._Len + 1]};
        if (_Data == nullptr) {
            return nullptr;
        }

        _Data[_Count_result._Len] = '\0';

        const auto _Result =
            __std_fs_convert_wide_to_narrow(_Code_page, _Input_as_wchar, _Input_len, _Data.get(), _Count_result._Len);
        if (_Result._Err != __std_win_error::_Success) {
            _Err = __std_tzdb_error::_Win_error;
            return nullptr;
        }

        return _Data.release();
    }

    _NODISCARD _STD unique_ptr<char16_t[]> _Get_canonical_id(
        const char16_t* _Id, const int32_t _Len, int32_t& _Result_len, __std_tzdb_error& _Err) noexcept {
        constexpr int32_t _Link_buf_len = 32;
        _STD unique_ptr<char16_t[]> _Link_buf{new (_STD nothrow) char16_t[_Link_buf_len]};
        if (_Link_buf == nullptr) {
            return nullptr;
        }

        UErrorCode _UErr{U_ZERO_ERROR};
        UBool _Is_system{};
        _Result_len = __icu_ucal_getCanonicalTimeZoneID(_Id, _Len, _Link_buf.get(), _Link_buf_len, &_Is_system, &_UErr);
        if (_UErr == U_BUFFER_OVERFLOW_ERROR && _Result_len > 0) {
            _Link_buf.reset(new (_STD nothrow) char16_t[_Result_len + 1]);
            if (_Link_buf == nullptr) {
                return nullptr;
            }

            _UErr = U_ZERO_ERROR; // reset error.
            _Result_len =
                __icu_ucal_getCanonicalTimeZoneID(_Id, _Len, _Link_buf.get(), _Result_len, &_Is_system, &_UErr);
            if (U_FAILURE(_UErr)) {
                _Err = __std_tzdb_error::_Icu_error;
                return nullptr;
            }
        } else if (U_FAILURE(_UErr) || _Result_len <= 0) {
            _Err = __std_tzdb_error::_Icu_error;
            return nullptr;
        }

        return _Link_buf;
    }

    _NODISCARD _STD unique_ptr<char16_t[]> _Get_default_timezone(
        int32_t& _Result_len, __std_tzdb_error& _Err) noexcept {
        constexpr int32_t _Name_buf_len = 32;
        _STD unique_ptr<char16_t[]> _Name_buf{new (_STD nothrow) char16_t[_Name_buf_len]};
        if (_Name_buf == nullptr) {
            return nullptr;
        }

        UErrorCode _UErr{U_ZERO_ERROR};
        _Result_len = __icu_ucal_getDefaultTimeZone(_Name_buf.get(), _Name_buf_len, &_UErr);
        if (_UErr == U_BUFFER_OVERFLOW_ERROR && _Result_len > 0) {
            _Name_buf.reset(new (_STD nothrow) char16_t[_Result_len + 1]);
            if (_Name_buf == nullptr) {
                return nullptr;
            }

            _UErr       = U_ZERO_ERROR; // reset error.
            _Result_len = __icu_ucal_getDefaultTimeZone(_Name_buf.get(), _Name_buf_len, &_UErr);
            if (U_FAILURE(_UErr)) {
                _Err = __std_tzdb_error::_Icu_error;
                return nullptr;
            }
        } else if (U_FAILURE(_UErr) || _Result_len <= 0) {
            _Err = __std_tzdb_error::_Icu_error;
            return nullptr;
        }

        return _Name_buf;
    }

    template <class _Ty, class _Dx>
    _NODISCARD _Ty* _Report_error(_STD unique_ptr<_Ty, _Dx>& _Info, __std_tzdb_error _Err) {
        _Info->_Err = _Err;
        return _Info.release();
    }

    template <class _Ty, class _Dx>
    _NODISCARD _Ty* _Propagate_error(_STD unique_ptr<_Ty, _Dx>& _Info) {
        // a bad_alloc returns nullptr and does not set __std_tzdb_error
        return _Info->_Err == __std_tzdb_error::_Success ? nullptr : _Info.release();
    }

} // unnamed namespace

_EXTERN_C

_NODISCARD __std_tzdb_time_zones_info* __stdcall __std_tzdb_get_time_zones() noexcept {
    // On exit---
    //    _Info == nullptr          --> bad_alloc
    //    _Info->_Err == _Win_error --> failed, call GetLastError()
    //    _Info->_Err == _Icu_error --> runtime_error interacting with ICU
    _STD unique_ptr<__std_tzdb_time_zones_info, decltype(&__std_tzdb_delete_time_zones)> _Info{
        new (_STD nothrow) __std_tzdb_time_zones_info{}, &__std_tzdb_delete_time_zones};
    if (_Info == nullptr) {
        return nullptr;
    }

    if (_Acquire_icu_functions() < _Icu_api_level::_Has_icu_addresses) {
        return _Report_error(_Info, __std_tzdb_error::_Win_error);
    }

    UErrorCode _UErr{U_ZERO_ERROR};
    _Info->_Version = __icu_ucal_getTZDataVersion(&_UErr);
    if (U_FAILURE(_UErr)) {
        return _Report_error(_Info, __std_tzdb_error::_Icu_error);
    }

    _STD unique_ptr<UEnumeration, decltype(&__icu_uenum_close)> _Enum{
        __icu_ucal_openTimeZoneIDEnumeration(USystemTimeZoneType::UCAL_ZONE_TYPE_ANY, nullptr, nullptr, &_UErr),
        &__icu_uenum_close};
    if (U_FAILURE(_UErr)) {
        return _Report_error(_Info, __std_tzdb_error::_Icu_error);
    }

    // uenum_count may be expensive but is required to pre-allocate arrays.
    const int32_t _Num_time_zones = __icu_uenum_count(_Enum.get(), &_UErr);
    if (U_FAILURE(_UErr)) {
        return _Report_error(_Info, __std_tzdb_error::_Icu_error);
    }

    _Info->_Num_time_zones = static_cast<size_t>(_Num_time_zones);
    // value-init to ensure __std_tzdb_delete_time_zones() cleanup is valid
    _Info->_Names = new (_STD nothrow) const char* [_Info->_Num_time_zones] {};
    if (_Info->_Names == nullptr) {
        return nullptr;
    }

    // value-init to ensure __std_tzdb_delete_time_zones() cleanup is valid
    _Info->_Links = new (_STD nothrow) const char* [_Info->_Num_time_zones] {};
    if (_Info->_Links == nullptr) {
        return nullptr;
    }

    for (size_t _Name_idx = 0; _Name_idx < _Info->_Num_time_zones; ++_Name_idx) {
        int32_t _Elem_len{};
        const auto* const _Elem = __icu_uenum_unext(_Enum.get(), &_Elem_len, &_UErr);
        if (U_FAILURE(_UErr) || _Elem == nullptr) {
            return _Report_error(_Info, __std_tzdb_error::_Icu_error);
        }

        _Info->_Names[_Name_idx] = _Allocate_wide_to_narrow(_Elem, _Elem_len, _Info->_Err);
        if (_Info->_Names[_Name_idx] == nullptr) {
            return _Propagate_error(_Info);
        }

        int32_t _Link_len{};
        const auto _Link = _Get_canonical_id(_Elem, _Elem_len, _Link_len, _Info->_Err);
        if (_Link == nullptr) {
            return _Propagate_error(_Info);
        }

        if (_STD u16string_view{_Elem, static_cast<size_t>(_Elem_len)}
            != _STD u16string_view{_Link.get(), static_cast<size_t>(_Link_len)}) {
            _Info->_Links[_Name_idx] = _Allocate_wide_to_narrow(_Link.get(), _Link_len, _Info->_Err);
            if (_Info->_Links[_Name_idx] == nullptr) {
                return _Propagate_error(_Info);
            }
        }
    }

    return _Info.release();
}

void __stdcall __std_tzdb_delete_time_zones(__std_tzdb_time_zones_info* const _Info) noexcept {
    if (_Info != nullptr) {
        if (_Info->_Names != nullptr) {
            for (size_t _Idx = 0; _Idx < _Info->_Num_time_zones; ++_Idx) {
                delete[] _Info->_Names[_Idx];
            }

            delete[] _Info->_Names;
            _Info->_Names = nullptr;
        }

        if (_Info->_Links != nullptr) {
            for (size_t _Idx = 0; _Idx < _Info->_Num_time_zones; ++_Idx) {
                delete[] _Info->_Links[_Idx];
            }

            delete[] _Info->_Links;
            _Info->_Links = nullptr;
        }
    }
}

_NODISCARD __std_tzdb_current_zone_info* __stdcall __std_tzdb_get_current_zone() noexcept {
    // On exit---
    //    _Info == nullptr          --> bad_alloc
    //    _Info->_Err == _Win_error --> failed, call GetLastError()
    //    _Info->_Err == _Icu_error --> runtime_error interacting with ICU
    _STD unique_ptr<__std_tzdb_current_zone_info, decltype(&__std_tzdb_delete_current_zone)> _Info{
        new (_STD nothrow) __std_tzdb_current_zone_info{}, &__std_tzdb_delete_current_zone};
    if (_Info == nullptr) {
        return nullptr;
    }

    if (_Acquire_icu_functions() < _Icu_api_level::_Has_icu_addresses) {
        return _Report_error(_Info, __std_tzdb_error::_Win_error);
    }

    int32_t _Id_len{};
    const auto _Id_name = _Get_default_timezone(_Id_len, _Info->_Err);
    if (_Id_name == nullptr) {
        return _Propagate_error(_Info);
    }

    _Info->_Tz_name = _Allocate_wide_to_narrow(_Id_name.get(), _Id_len, _Info->_Err);
    if (_Info->_Tz_name == nullptr) {
        return _Propagate_error(_Info);
    }

    return _Info.release();
}

void __stdcall __std_tzdb_delete_current_zone(__std_tzdb_current_zone_info* const _Info) noexcept {
    if (_Info) {
        delete[] _Info->_Tz_name;
        _Info->_Tz_name = nullptr;
    }
}

__std_tzdb_registry_leap_info* __stdcall __std_tzdb_get_reg_leap_seconds(
    const size_t prev_reg_ls_size, size_t* const current_reg_ls_size) noexcept {
    // On exit---
    //    *current_reg_ls_size <= prev_reg_ls_size, reg_ls_data == nullptr --> no new data
    //    *current_reg_ls_size >  prev_reg_ls_size, reg_ls_data != nullptr --> new data, successfully read
    //    *current_reg_ls_size == 0,                reg_ls_data != nullptr --> new data, failed reading
    //    *current_reg_ls_size >  prev_reg_ls_size, reg_ls_data == nullptr --> new data, failed allocation

    constexpr auto reg_key_name    = LR"(SYSTEM\CurrentControlSet\Control\LeapSecondInformation)";
    constexpr auto reg_subkey_name = L"LeapSeconds";
    *current_reg_ls_size           = 0;
    HKEY leap_sec_key              = nullptr;

    LSTATUS status = RegOpenKeyExW(HKEY_LOCAL_MACHINE, reg_key_name, 0, KEY_READ, &leap_sec_key);
    if (status != ERROR_SUCCESS) {
        // May not exist on older systems. Treat this as equivalent to the key existing but with no data.
        return nullptr;
    }

    DWORD byte_size = 0;
    status          = RegQueryValueExW(leap_sec_key, reg_subkey_name, nullptr, nullptr, nullptr, &byte_size);
    static_assert(sizeof(__std_tzdb_registry_leap_info) == 12);
    const auto ls_size   = byte_size / 12;
    *current_reg_ls_size = ls_size;

    __std_tzdb_registry_leap_info* reg_ls_data = nullptr;
    if ((status == ERROR_SUCCESS || status == ERROR_MORE_DATA) && ls_size > prev_reg_ls_size) {
        try {
            reg_ls_data = new __std_tzdb_registry_leap_info[ls_size];
            status      = RegQueryValueExW(
                leap_sec_key, reg_subkey_name, nullptr, nullptr, reinterpret_cast<LPBYTE>(reg_ls_data), &byte_size);
            if (status != ERROR_SUCCESS) {
                *current_reg_ls_size = 0;
            }
        } catch (...) {
        }
    }

    RegCloseKey(leap_sec_key);
    if (status != ERROR_SUCCESS) {
        SetLastError(status);
    }

    return reg_ls_data;
}

void __stdcall __std_tzdb_delete_reg_leap_seconds(__std_tzdb_registry_leap_info* _Rlsi) noexcept {
    delete[] _Rlsi;
}

_NODISCARD void* __stdcall __std_calloc_crt(const size_t count, const size_t size) noexcept {
    return _calloc_crt(count, size);
}

void __stdcall __std_free_crt(void* p) noexcept {
    _free_crt(p);
}

_END_EXTERN_C