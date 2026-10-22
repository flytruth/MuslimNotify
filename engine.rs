//! Prayer-time computation with local-civil-date anchoring.
//!
//! See the crate root docs for why the anchor resolution exists. In short:
//! `adhan` reports hours on a UTC day (wrapped into 0..24), while users think
//! in *local* civil dates. We anchor on the UTC date containing the local
//! midnight of the requested date and lift each wrapped reading back into the
//! local day, yielding seconds since local midnight — a representation the UI
//! can format without ever touching timezone rules again.

use crate::model::{AsrMadhab, CalcMethod, HighLatRule, DailyTimes, PRAYER_COUNT};

/// Errors the engine can produce. FFI maps these onto small integer codes.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TimeAnchorError {
    /// Civil date is not a valid Gregorian date (e.g. 2026-02-30).
    InvalidDate,
    /// Latitude/longitude outside the WGS-84 sane range.
    InvalidCoordinates,
}

const SECS_PER_DAY: i64 = 86_400;

/// Days since 1970-01-01 for a proleptic Gregorian date
/// (Howard Hinnant's `days_from_civil`, public domain).
fn days_from_civil(y: i64, m: i64, d: i64) -> i64 {
    let y = if m <= 2 { y - 1 } else { y };
    let era = if y >= 0 { y } else { y - 399 } / 400;
    let yoe = y - era * 400;
    let mp = if m > 2 { m - 3 } else { m + 9 };
    let doy = (153 * mp + 2) / 5 + d - 1;
    let doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    era * 146_097 + doe - 719_468
}

/// Inverse of [`days_from_civil`] for validation and tests
/// (Hinnant's `civil_from_days`).
#[allow(dead_code)]
fn civil_from_days(z: i64) -> (i64, i64, i64) {
    let z = z + 719_468;
    let era = if z >= 0 { z } else { z - 146_096 } / 146_097;
    let doe = z - era * 146_097;
    let yoe = (doe - doe / 1460 + doe / 36524 - doe / 146_096) / 365;
    let y = yoe + era * 400;
    let doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    let mp = (5 * doy + 2) / 153;
    let d = doy - (153 * mp + 2) / 5 + 1;
    let m = if mp < 10 { mp + 3 } else { mp - 9 };
    (if m <= 2 { y + 1 } else { y }, m, d)
}

/// Validate a Gregorian civil date by round-tripping through the day count.
fn is_valid_date(y: i32, m: u32, d: u32) -> bool {
    if !(1..=12).contains(&m) || !(1..=31).contains(&d) || !(1..=9999).contains(&y) {
        return false;
    }
    let days = days_from_civil(y as i64, m as i64, d as i64);
    civil_from_days(days) == (y as i64, m as i64, d as i64)
}

/// Build the adhan parameter set for a request.
fn build_parameters(
    method: CalcMethod,
    madhab: AsrMadhab,
    rule: HighLatRule,
    latitude: f64,
) -> adhan::Parameters {
    let mut params = adhan::Parameters::for_method(method.adhan_method());
    params.madhab = madhab.adhan_madhab();
    params.high_latitude_rule = match rule {
        HighLatRule::Auto => adhan::HighLatitudeRule::recommended(latitude),
        HighLatRule::MiddleOfTheNight => adhan::HighLatitudeRule::MiddleOfTheNight,
        HighLatRule::SeventhOfTheNight => adhan::HighLatitudeRule::SeventhOfTheNight,
        HighLatRule::TwilightAngle => adhan::HighLatitudeRule::TwilightAngle,
    };
    params
}

/// Lift one UTC-wrapped `(hour, minute, second)` reading onto the local civil
/// day. Returns seconds since local midnight.
///
/// `utc_date_midnight` is the epoch of the anchor UTC date's midnight,
/// `local_midnight` the epoch of the caller's local midnight. The reading may
/// describe an instant on the anchor day or 24h beyond it; exactly one of the
/// two candidates lands inside the local day, which is the window that must
/// contain the prayer.
fn lift_to_local_day(
    reading: (i64, u32, u32),
    utc_date_midnight: i64,
    local_midnight: i64,
) -> i64 {
    let wrapped = reading.0 * 3600 + reading.1 as i64 * 60 + reading.2 as i64;
    let base = utc_date_midnight + wrapped;
    let mut rel = base - local_midnight;
    if rel < 0 {
        rel += SECS_PER_DAY;
    } else if rel >= SECS_PER_DAY {
        rel -= SECS_PER_DAY;
    }
    rel
}

/// Compute all six boundaries for a **local civil date**.
///
/// * `tz_offset_secs` — seconds east of UTC in effect on that local date
///   (the frontend asks its platform for this once per day, so DST is handled
///   by the OS, not by us).
/// * Result seconds are relative to local midnight of that date; `None` means
///   the sun never crosses the relevant line that day (polar summer/winter).
///
/// # Errors
/// [`TimeAnchorError::InvalidDate`] or [`TimeAnchorError::InvalidCoordinates`].
pub fn compute_local_times(
    year: i32,
    month: u32,
    day: u32,
    tz_offset_secs: i64,
    latitude: f64,
    longitude: f64,
    method: CalcMethod,
    madhab: AsrMadhab,
    rule: HighLatRule,
) -> Result<DailyTimes, TimeAnchorError> {
    if !is_valid_date(year, month, day) {
        return Err(TimeAnchorError::InvalidDate);
    }
    if !(-90.0..=90.0).contains(&latitude) || !(-180.0..=180.0).contains(&longitude) {
        return Err(TimeAnchorError::InvalidCoordinates);
    }

    // The UTC date that contains the local midnight of the requested day.
    // `anchor_local_midnight` is the TRUE epoch of local midnight (wall 00:00
    // minus the timezone offset); its UTC date anchors the adhan call, and
    // `lift_to_local_day` then re-bases every wrapped reading onto the local
    // day.
    let local_midnight = days_from_civil(year as i64, month as i64, day as i64) * SECS_PER_DAY;
    let anchor_local_midnight = local_midnight - tz_offset_secs;
    let anchor_utc_days = anchor_local_midnight.div_euclid(SECS_PER_DAY);
    let anchor_midnight = anchor_utc_days * SECS_PER_DAY;
    let (ay, am, ad) = civil_from_days(anchor_utc_days);

    let params = build_parameters(method, madhab, rule, latitude);
    let times = adhan::prayer_times(ay as i32, am as u32, ad as u32, latitude, longitude, &params);

    let lift = |r: Option<(i64, u32, u32)>| {
        r.map(|reading| lift_to_local_day(reading, anchor_midnight, anchor_local_midnight))
    };

    Ok(DailyTimes {
        times: [
            lift(times.fajr),
            lift(times.sunrise),
            lift(times.dhuhr),
            lift(times.asr),
            lift(times.maghrib),
            lift(times.isha),
        ],
    })
}

/// Format seconds-since-local-midnight as `HH:MM` (24h) or `h:MM AM/PM` (12h).
pub fn format_clock(secs: i64, use_24h: bool) -> String {
    let s = secs.rem_euclid(SECS_PER_DAY);
    let h24 = (s / 3600) as u32;
    let m = ((s % 3600) / 60) as u32;
    if use_24h {
        format!("{h24:02}:{m:02}")
    } else {
        let ampm = if h24 < 12 { "AM" } else { "PM" };
        let h12 = match h24 % 12 {
            0 => 12,
            h => h,
        };
        format!("{h12}:{m:02} {ampm}")
    }
}

/// Convenience for the CLI and tests: full day as strings.
pub fn format_daily(times: &DailyTimes, use_24h: bool) -> [String; PRAYER_COUNT] {
    let mut out = [const { String::new() }; PRAYER_COUNT];
    for (i, slot) in times.times.iter().enumerate() {
        out[i] = match slot {
            Some(s) => format_clock(*s, use_24h),
            None => "—".to_string(),
        };
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::model::prayer_index as pi;

    /// Makkah, Umm al-Qura: the reference city/authority pair.
    /// Solar noon in Makkah is ~12:15-12:20 local (UTC+3) in October.
    #[test]
    fn makkah_sanity() {
        let t = compute_local_times(
            2026, 10, 22, 3 * 3600, 21.4225, 39.8262, CalcMethod::UmmAlQura, AsrMadhab::Shafi,
            HighLatRule::Auto,
        )
        .expect("valid");
        let f = format_daily(&t, true);
        // Orderings that must always hold.
        assert!(t.times[pi::FAJR].unwrap() < t.times[pi::SUNRISE].unwrap());
        assert!(t.times[pi::SUNRISE].unwrap() < t.times[pi::DHUHR].unwrap());
        assert!(t.times[pi::DHUHR].unwrap() < t.times[pi::ASR].unwrap());
        assert!(t.times[pi::ASR].unwrap() < t.times[pi::MAGHRIB].unwrap());
        assert!(t.times[pi::MAGHRIB].unwrap() < t.times[pi::ISHA].unwrap());
        // Umm al-Qura Isha is a fixed 90 minutes after Maghrib.
        let diff = t.times[pi::ISHA].unwrap() - t.times[pi::MAGHRIB].unwrap();
        assert_eq!(diff, 90 * 60, "UmmAlQura isha interval");
        // Dhuhr near solar noon; Fajr early morning; Maghrib early evening.
        let dhuhr = t.times[pi::DHUHR].unwrap();
        assert!((11 * 3600 + 50 * 60..=12 * 3600 + 45 * 60).contains(&dhuhr), "dhuhr {dhuhr}");
        assert!((4 * 3600..=6 * 3600).contains(&t.times[pi::FAJR].unwrap()));
        assert!((17 * 3600..=19 * 3600).contains(&t.times[pi::MAGHRIB].unwrap()));
        assert_eq!(f[pi::FAJR], format_clock(t.times[pi::FAJR].unwrap(), true));
    }

    /// Apia, Samoa (UTC+13): the classic case where the true UTC instant of a
    /// prayer on local date D lands on the previous UTC date. The lifted times
    /// must still read as sane local wall-clock times of date D.
    #[test]
    fn samoa_far_east_date_integrity() {
        let t = compute_local_times(
            2026, 10, 22, 13 * 3600, -13.8333, -171.7667, CalcMethod::MuslimWorldLeague,
            AsrMadhab::Shafi, HighLatRule::Auto,
        )
        .expect("valid");
        assert!(t.times[pi::FAJR].unwrap() < t.times[pi::SUNRISE].unwrap());
        assert!(t.times[pi::SUNRISE].unwrap() < t.times[pi::DHUHR].unwrap());
        assert!(t.times[pi::DHUHR].unwrap() < t.times[pi::MAGHRIB].unwrap());
        assert!(t.times[pi::MAGHRIB].unwrap() < t.times[pi::ISHA].unwrap());
        // Solar noon in Apia is ~12:0x local; Dhuhr must not drift to the
        // afternoon, which is the symptom of a one-day anchor error.
        let dhuhr = t.times[pi::DHUHR].unwrap();
        assert!(
            dhuhr >= 11 * 3600 && dhuhr <= 13 * 3600,
            "Apia dhuhr drifted: {}s",
            dhuhr
        );
    }

    /// Honolulu (UTC-10): extreme west, Fajr instant lands on the *next* UTC
    /// date. Same date-integrity assertions.
    #[test]
    fn honolulu_far_west_date_integrity() {
        let t = compute_local_times(
            2026, 10, 22, -10 * 3600, 21.3069, -157.8583, CalcMethod::MuslimWorldLeague,
            AsrMadhab::Shafi, HighLatRule::Auto,
        )
        .expect("valid");
        assert!(t.times[pi::FAJR].unwrap() < t.times[pi::SUNRISE].unwrap());
        assert!(t.times[pi::SUNRISE].unwrap() < t.times[pi::DHUHR].unwrap());
        assert!(t.times[pi::MAGHRIB].unwrap() < t.times[pi::ISHA].unwrap());
        let dhuhr = t.times[pi::DHUHR].unwrap();
        assert!(
            dhuhr >= 11 * 3600 && dhuhr <= 13 * 3600,
            "Honolulu dhuhr drifted: {}s",
            dhuhr
        );
    }

    /// Kiritimati (UTC+14) — the easternmost inhabited timezone.
    #[test]
    fn kiritimati_utc_plus_14() {
        let t = compute_local_times(
            2026, 6, 21, 14 * 3600, 1.9833, -157.4667, CalcMethod::MuslimWorldLeague,
            AsrMadhab::Shafi, HighLatRule::Auto,
        )
        .expect("valid");
        assert!(t.times[pi::FAJR].unwrap() < t.times[pi::SUNRISE].unwrap());
        assert!(t.times[pi::SUNRISE].unwrap() < t.times[pi::MAGHRIB].unwrap());
        let dhuhr = t.times[pi::DHUHR].unwrap();
        assert!(dhuhr >= 11 * 3600 && dhuhr <= 13 * 3600, "dhuhr {dhuhr}");
    }

    /// Nuuk area / high latitude: rule must not crash; polar winter may give
    /// None for Fajr/Isha but Dhuhr/Maghrib exist year-round below ~72.5°N.
    #[test]
    fn high_latitude_never_panics() {
        let t = compute_local_times(
            2026, 6, 21, 2 * 3600, 64.1836, -51.7214, CalcMethod::MuslimWorldLeague,
            AsrMadhab::Shafi, HighLatRule::Auto,
        )
        .expect("valid");
        assert!(t.times[pi::DHUHR].is_some() || t.times[pi::MAGHRIB].is_some());
        // No time may be negative or >= 24h.
        for slot in t.times.iter().flatten() {
            assert!(*slot >= 0 && *slot < SECS_PER_DAY);
        }
    }

    /// Longyearbyen (78.2°N) polar night: the sun never rises, so sunrise is
    /// legitimately `None`, while Fajr/Isha fall back to the night-portion
    /// rule exactly like the adhan reference implementation.
    #[test]
    fn polar_night_yields_none() {
        let t = compute_local_times(
            2026, 12, 21, 1 * 3600, 78.2232, 15.6267, CalcMethod::MuslimWorldLeague,
            AsrMadhab::Shafi, HighLatRule::SeventhOfTheNight,
        )
        .expect("valid");
        assert!(t.times[pi::SUNRISE].is_none(), "polar night sunrise");
        // Night-portion fallback keeps Fajr/Isha defined and ordered.
        let fajr = t.times[pi::FAJR].expect("night-portion fajr");
        let isha = t.times[pi::ISHA].expect("night-portion isha");
        assert!(fajr < isha);
        for slot in t.times.iter().flatten() {
            assert!(*slot >= 0 && *slot < SECS_PER_DAY);
        }
    }

    /// Rejections.
    #[test]
    fn rejects_bad_input() {
        assert_eq!(
            compute_local_times(2026, 2, 30, 0, 0.0, 0.0, CalcMethod::Egyptian,
                AsrMadhab::Shafi, HighLatRule::Auto),
            Err(TimeAnchorError::InvalidDate)
        );
        assert_eq!(
            compute_local_times(2026, 1, 1, 0, 91.0, 0.0, CalcMethod::Egyptian,
                AsrMadhab::Shafi, HighLatRule::Auto),
            Err(TimeAnchorError::InvalidCoordinates)
        );
    }

    /// DST does not apply in the engine — the caller passes the offset — but
    /// a shifted offset must move the results consistently: local times shift
    /// by exactly the offset change.
    #[test]
    fn offset_change_moves_wall_clock() {
        let a = compute_local_times(2026, 7, 15, 2 * 3600, 48.8566, 2.3522,
            CalcMethod::MuslimWorldLeague, AsrMadhab::Shafi, HighLatRule::Auto).unwrap();
        let b = compute_local_times(2026, 7, 15, 3 * 3600, 48.8566, 2.3522,
            CalcMethod::MuslimWorldLeague, AsrMadhab::Shafi, HighLatRule::Auto).unwrap();
        let diff = b.times[pi::FAJR].unwrap() - a.times[pi::FAJR].unwrap();
        // Paris: +1h offset moves local wall clock +1h for the same instant,
        // modulo the one-day anchor flip at the extremes.
        assert!(
            diff == 3600 || diff == 3600 - SECS_PER_DAY || diff == 3600 + SECS_PER_DAY,
            "unexpected shift {diff}"
        );
    }

    /// Hinnant round-trip across era boundaries.
    #[test]
    fn civil_roundtrip() {
        for &(y, m, d) in &[(1970, 1, 1), (2000, 2, 29), (2026, 10, 22), (1900, 3, 1)] {
            let days = days_from_civil(y, m, d);
            assert_eq!(civil_from_days(days), (y, m, d));
        }
        assert_eq!(days_from_civil(1970, 1, 1), 0);
    }

    /// Clock formatting, 12h and 24h.
    #[test]
    fn clock_formatting() {
        assert_eq!(format_clock(0, true), "00:00");
        assert_eq!(format_clock(5 * 3600 + 30 * 60, true), "05:30");
        assert_eq!(format_clock(13 * 3600 + 5 * 60, true), "13:05");
        assert_eq!(format_clock(5 * 3600 + 30 * 60, false), "5:30 AM");
        assert_eq!(format_clock(13 * 3600 + 5 * 60, false), "1:05 PM");
        assert_eq!(format_clock(12 * 3600, false), "12:00 PM");
        assert_eq!(format_clock(0, false), "12:00 AM");
    }
}
