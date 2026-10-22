//! C-ABI surface consumed by the C++ Qt frontend.
//!
//! Design rules:
//! * Only plain integers, doubles and `*const c_char` cross the boundary.
//! * Every returned string is a `'static` literal — the caller never frees.
//! * Errors are small integer codes, documented in `include/muslimnotify.h`.
//! * Prayer times leave as `i64` seconds since local midnight (`-1` = none),
//!   so the UI never touches timezone math.

use std::ffi::CStr;
use std::os::raw::{c_char, c_int, c_longlong};

use mn_core::model::{prayer_index, AsrMadhab, CalcMethod, HighLatRule, PRAYER_COUNT};
use mn_core::{compute_local_times, format_clock};

/// NUL-terminated mirrors of the core name tables. A compile-time test below
/// pins every entry to the mn-core source of truth, so the two can never
/// drift apart.
const PRAYER_EN: [&CStr; PRAYER_COUNT] = [
    c"Fajr", c"Sunrise", c"Dhuhr", c"Asr", c"Maghrib", c"Isha",
];
const PRAYER_AR: [&CStr; PRAYER_COUNT] = [
    c"الفجر", c"الشروق", c"الظهر", c"العصر", c"المغرب", c"العشاء",
];
const METHOD_DISPLAY: [&CStr; 12] = [
    c"Muslim World League",
    c"Egyptian General Authority of Survey",
    c"University of Islamic Sciences, Karachi",
    c"Umm al-Qura, Makkah",
    c"Dubai",
    c"Moonsighting Committee",
    c"Islamic Society of North America (ISNA)",
    c"Kuwait",
    c"Qatar",
    c"Singapore",
    c"Institute of Geophysics, University of Tehran",
    c"Diyanet, Turkey",
];
const METHOD_SHORT: [&CStr; 12] = [
    c"MWL", c"EGYPT", c"KARACHI", c"MAKKAH", c"DUBAI", c"MOONSHOT",
    c"ISNA", c"KUWAIT", c"QATAR", c"SINGAPORE", c"TEHRAN", c"TURKEY",
];
const HIGH_LAT_NAMES: [&CStr; 4] = [
    c"Auto (recommended for latitude)",
    c"Middle of the night",
    c"Seventh of the night",
    c"Twilight angle",
];

/// Map a method id onto its index in the stable ALL table.
fn method_idx(id: c_int) -> usize {
    CalcMethod::ALL
        .iter()
        .position(|m| *m as i32 == id)
        .unwrap_or(0)
}

/// Result codes for [`mn_compute_times`].
pub const MN_OK: c_int = 0;
pub const MN_ERR_INVALID_DATE: c_int = 1;
pub const MN_ERR_INVALID_COORDS: c_int = 2;
/// Sentinel written into `out_secs` slots that have no valid time.
pub const MN_NO_TIME: c_longlong = -1;

/// ABI-stable version string of the Rust core.
#[no_mangle]
pub extern "C" fn mn_version() -> *const c_char {
    concat!(env!("CARGO_PKG_VERSION"), "\0").as_ptr() as *const c_char
}

/// Number of tracked boundaries (5 prayers + sunrise).
#[no_mangle]
pub extern "C" fn mn_prayer_count() -> c_int {
    PRAYER_COUNT as c_int
}

/// English name of slot `i` (Fajr, Sunrise, ...).
///
/// # Safety
/// `i` must be in `0..mn_prayer_count()`.
#[no_mangle]
pub unsafe extern "C" fn mn_prayer_name_en(i: c_int) -> *const c_char {
    PRAYER_EN
        .get(i as usize)
        .map(|c| c.as_ptr())
        .unwrap_or(c"?".as_ptr())
        .cast()
}

/// Arabic name of slot `i` (الفجر, الشروق, ...).
///
/// # Safety
/// `i` must be in `0..mn_prayer_count()`.
#[no_mangle]
pub unsafe extern "C" fn mn_prayer_name_ar(i: c_int) -> *const c_char {
    PRAYER_AR
        .get(i as usize)
        .map(|c| c.as_ptr())
        .unwrap_or(c"?".as_ptr())
        .cast()
}

/// Number of calculation methods exposed in the UI.
#[no_mangle]
pub extern "C" fn mn_method_count() -> c_int {
    CalcMethod::ALL.len() as c_int
}

/// Stable method id at UI combo-box position `idx`.
///
/// # Safety
/// `idx` must be in `0..mn_method_count()`.
#[no_mangle]
pub unsafe extern "C" fn mn_method_id(idx: c_int) -> c_int {
    match CalcMethod::ALL.get(idx as usize) {
        Some(m) => *m as c_int,
        None => 0,
    }
}

/// Human-readable authority name for method `id`.
#[no_mangle]
pub extern "C" fn mn_method_display_name(id: c_int) -> *const c_char {
    METHOD_DISPLAY[method_idx(id)].as_ptr().cast()
}

/// Short method tag (MWL, MAKKAH, ISNA, ...).
#[no_mangle]
pub extern "C" fn mn_method_short_name(id: c_int) -> *const c_char {
    METHOD_SHORT[method_idx(id)].as_ptr().cast()
}

/// Number of high-latitude rules (including the Auto entry).
#[no_mangle]
pub extern "C" fn mn_high_lat_rule_count() -> c_int {
    4
}

/// Display name of high-latitude rule `id` (0=Auto, 1=Middle, 2=Seventh, 3=Twilight).
#[no_mangle]
pub extern "C" fn mn_high_lat_rule_name(id: c_int) -> *const c_char {
    let idx = HighLatRule::from_id(id) as usize;
    HIGH_LAT_NAMES[idx.min(3)].as_ptr().cast()
}

/// Format seconds-since-local-midnight as a clock string.
///
/// `use_24h != 0` gives `HH:MM`; otherwise `h:MM AM/PM`. Writes at most
/// `cap` bytes including the NUL terminator into `out`.
///
/// # Safety
/// `out` must be writable for `cap` bytes.
#[no_mangle]
pub unsafe extern "C" fn mn_format_clock(
    secs: c_longlong,
    use_24h: c_int,
    out: *mut c_char,
    cap: usize,
) -> c_int {
    if out.is_null() || cap < 2 {
        return 1;
    }
    let s = format_clock(secs, use_24h != 0);
    let bytes = s.as_bytes();
    let n = bytes.len().min(cap - 1);
    std::ptr::copy_nonoverlapping(bytes.as_ptr() as *const c_char, out, n);
    *out.add(n) = 0;
    0
}

/// Compute the six boundaries for one local civil date.
///
/// `tz_offset_secs` is the UTC offset in effect on that date (seconds east).
/// On success writes 6 `i64` values into `out_secs` — seconds since local
/// midnight, or `MN_NO_TIME` when the sun never crosses the line — and
/// returns [`MN_OK`].
///
/// # Safety
/// `out_secs` must be writable for 6 `i64`.
#[no_mangle]
pub unsafe extern "C" fn mn_compute_times(
    year: c_int,
    month: c_int,
    day: c_int,
    tz_offset_secs: c_longlong,
    latitude: f64,
    longitude: f64,
    method_id: c_int,
    madhab_id: c_int,
    high_lat_id: c_int,
    out_secs: *mut c_longlong,
) -> c_int {
    if out_secs.is_null() {
        return MN_ERR_INVALID_DATE;
    }
    let month_u = if (1..=12).contains(&month) { month as u32 } else { 0 };
    let day_u = if (1..=31).contains(&day) { day as u32 } else { 0 };
    let result = compute_local_times(
        year,
        month_u,
        day_u,
        tz_offset_secs,
        latitude,
        longitude,
        CalcMethod::from_id(method_id),
        AsrMadhab::from_id(madhab_id),
        HighLatRule::from_id(high_lat_id),
    );
    match result {
        Ok(times) => {
            for i in 0..PRAYER_COUNT {
                let v = times.times[i].unwrap_or(MN_NO_TIME);
                *out_secs.add(i) = v;
            }
            MN_OK
        }
        Err(mn_core::TimeAnchorError::InvalidDate) => {
            for i in 0..PRAYER_COUNT {
                *out_secs.add(i) = MN_NO_TIME;
            }
            MN_ERR_INVALID_DATE
        }
        Err(mn_core::TimeAnchorError::InvalidCoordinates) => {
            for i in 0..PRAYER_COUNT {
                *out_secs.add(i) = MN_NO_TIME;
            }
            MN_ERR_INVALID_COORDS
        }
    }
}

/// Index helpers re-exported for the header docs.
#[no_mangle]
pub extern "C" fn mn_index_fajr() -> c_int {
    prayer_index::FAJR as c_int
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn compute_via_ffi_contract() {
        let mut out = [0i64; PRAYER_COUNT];
        // 2026-10-22, Makkah, UTC+3, UmmAlQura(3), Shafi(0), Auto(0).
        let rc = unsafe {
            mn_compute_times(2026, 10, 22, 10_800, 21.4225, 39.8262, 3, 0, 0, out.as_mut_ptr())
        };
        assert_eq!(rc, MN_OK);
        assert!(out[prayer_index::FAJR] > 0);
        assert!(out[prayer_index::MAGHRIB] > out[prayer_index::ASR]);
        // 12h clock string.
        let mut buf = [0u8; 32];
        let rc = unsafe {
            mn_format_clock(out[prayer_index::DHUHR], 0, buf.as_mut_ptr() as *mut c_char, 32)
        };
        assert_eq!(rc, 0);
        let s = std::ffi::CStr::from_bytes_until_nul(&buf).unwrap().to_str().unwrap();
        assert!(s.ends_with("PM"), "dhuhr should be PM, got {s}");
    }

    #[test]
    fn error_codes_propagate() {
        let mut out = [0i64; PRAYER_COUNT];
        let rc = unsafe {
            mn_compute_times(2026, 2, 30, 0, 21.0, 39.0, 0, 0, 0, out.as_mut_ptr())
        };
        assert_eq!(rc, MN_ERR_INVALID_DATE);
        let rc = unsafe {
            mn_compute_times(2026, 1, 1, 0, 999.0, 39.0, 0, 0, 0, out.as_mut_ptr())
        };
        assert_eq!(rc, MN_ERR_INVALID_COORDS);
        for v in out {
            assert_eq!(v, MN_NO_TIME);
        }
    }

    #[test]
    fn method_table_stable() {
        assert_eq!(mn_method_count(), 12);
        // Identity mapping by design: UI position == stable id.
        for pos in 0..mn_method_count() {
            assert_eq!(unsafe { mn_method_id(pos) }, pos);
        }
        // Every FFI name must match the mn-core source of truth exactly.
        for m in CalcMethod::ALL {
            let id = m as c_int;
            let display = unsafe { CStr::from_ptr(mn_method_display_name(id)) }
                .to_str()
                .unwrap();
            let short = unsafe { CStr::from_ptr(mn_method_short_name(id)) }
                .to_str()
                .unwrap();
            assert_eq!(display, m.display_name(), "display name for id {id}");
            assert_eq!(short, m.short_name(), "short name for id {id}");
        }
        // Unknown ids fall back to MWL rather than crashing the UI.
        let fallback = unsafe { CStr::from_ptr(mn_method_display_name(999)) };
        assert_eq!(fallback.to_str().unwrap(), "Muslim World League");
        // Prayer names match too.
        for (i, en) in mn_core::PRAYER_NAMES_EN.iter().enumerate() {
            let via_ffi = unsafe { CStr::from_ptr(mn_prayer_name_en(i as c_int)) };
            assert_eq!(via_ffi.to_str().unwrap(), *en);
        }
        for (i, ar) in mn_core::PRAYER_NAMES_AR.iter().enumerate() {
            let via_ffi = unsafe { CStr::from_ptr(mn_prayer_name_ar(i as c_int)) };
            assert_eq!(via_ffi.to_str().unwrap(), *ar);
        }
    }
}
