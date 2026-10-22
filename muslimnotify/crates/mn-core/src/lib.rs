//! MuslimNotify core — prayer time engine.
//!
//! Wraps the [`adhan`] crate (the Rust port of the battle-tested batoulapps
//! reference implementation) and converts its UTC-anchored output into
//! **seconds since local midnight** for the caller's civil date.
//!
//! Why the conversion layer exists: `adhan::prayer_times` takes a Gregorian
//! date and returns hours on that *UTC* day, wrapped into 0..24 with
//! `rem_euclid`. For eastern timezones (UTC+10..+14) the true instant of a
//! prayer on local date `D` lands on the *previous* UTC date, and for western
//! timezones late prayers can spill into the next one. Passing the local date
//! naively therefore shifts results by a day at the extremes. The fix here is
//! geometric: anchor on the UTC date that contains the local midnight of the
//! requested civil date, then resolve each wrapped reading against exactly one
//! candidate in `[local_midnight, local_midnight + 24h)` — the window that by
//! definition contains every prayer of that local day.

pub mod model;
pub mod engine;

pub use engine::{compute_local_times, format_clock, format_daily, TimeAnchorError};
pub use model::{prayer_index, AsrMadhab, CalcMethod, DailyTimes, HighLatRule, PRAYER_COUNT, PRAYER_NAMES_AR, PRAYER_NAMES_EN};
