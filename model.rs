//! Public settings model shared by the Rust core, the FFI layer, the CLI and
//! the C++ frontend. Kept dependency-free so every binding stays trivial.

/// Number of tracked daytime boundaries. The five prayers plus sunrise,
/// which is not prayed but is needed for scheduling and display.
pub const PRAYER_COUNT: usize = 6;

/// Index of each slot inside [`DailyTimes::times`].
pub mod prayer_index {
    pub const FAJR: usize = 0;
    pub const SUNRISE: usize = 1;
    pub const DHUHR: usize = 2;
    pub const ASR: usize = 3;
    pub const MAGHRIB: usize = 4;
    pub const ISHA: usize = 5;
}

/// All six boundaries for one local civil date, each as seconds since local
/// midnight. `None` when the sun never crosses the relevant line (polar day
/// or night). Slot order: Fajr, Sunrise, Dhuhr, Asr, Maghrib, Isha.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub struct DailyTimes {
    pub times: [Option<i64>; PRAYER_COUNT],
}

impl DailyTimes {
    /// The five prayers, skipping sunrise. (index, seconds) pairs.
    pub fn prayers(&self) -> impl Iterator<Item = (usize, i64)> + '_ {
        self.times
            .iter()
            .enumerate()
            .filter(|(i, t)| *i != prayer_index::SUNRISE && t.is_some())
            .map(|(i, t)| (i, t.unwrap()))
    }

    /// The prayer currently in progress given seconds since local midnight:
    /// the latest prayer whose time has passed. Before Fajr, Isha of *this*
    /// civil-day table is still "current" until Fajr arrives — the caller
    /// handles that pre-dawn case; here we only scan the day itself.
    pub fn current_at(&self, secs: i64) -> Option<usize> {
        self.prayers()
            .filter(|(_, t)| *t <= secs)
            .map(|(i, _)| i)
            .last()
    }

    /// The next prayer after `secs` seconds since local midnight. `None` when
    /// the day is exhausted (Isha passed) or all times missing.
    pub fn next_at(&self, secs: i64) -> Option<usize> {
        self.prayers()
            .find(|(_, t)| *t > secs)
            .map(|(i, _)| i)
    }
}

/// The five daily prayer names in English and Arabic. Indexes line up with
/// [`prayer_index`] minus the sunrise slot.
pub const PRAYER_NAMES_EN: [&str; PRAYER_COUNT] = [
    "Fajr", "Sunrise", "Dhuhr", "Asr", "Maghrib", "Isha",
];
pub const PRAYER_NAMES_AR: [&str; PRAYER_COUNT] = [
    "الفجر", "الشروق", "الظهر", "العصر", "المغرب", "العشاء",
];

/// Calculation authority. Discriminants are part of the FFI contract with the
/// C++ frontend and are persisted in settings files, so they must never be
/// reordered.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(i32)]
pub enum CalcMethod {
    MuslimWorldLeague = 0,
    Egyptian = 1,
    Karachi = 2,
    UmmAlQura = 3,
    Dubai = 4,
    MoonsightingCommittee = 5,
    NorthAmerica = 6,
    Kuwait = 7,
    Qatari = 8,
    Singapore = 9,
    Tehran = 10,
    Turkey = 11,
}

impl CalcMethod {
    /// Every method exposed in the UI, in stable order.
    pub const ALL: [CalcMethod; 12] = [
        Self::MuslimWorldLeague,
        Self::Egyptian,
        Self::Karachi,
        Self::UmmAlQura,
        Self::Dubai,
        Self::MoonsightingCommittee,
        Self::NorthAmerica,
        Self::Kuwait,
        Self::Qatari,
        Self::Singapore,
        Self::Tehran,
        Self::Turkey,
    ];

    /// Human-readable authority name shown in the settings dialog.
    pub const fn display_name(self) -> &'static str {
        match self {
            Self::MuslimWorldLeague => "Muslim World League",
            Self::Egyptian => "Egyptian General Authority of Survey",
            Self::Karachi => "University of Islamic Sciences, Karachi",
            Self::UmmAlQura => "Umm al-Qura, Makkah",
            Self::Dubai => "Dubai",
            Self::MoonsightingCommittee => "Moonsighting Committee",
            Self::NorthAmerica => "Islamic Society of North America (ISNA)",
            Self::Kuwait => "Kuwait",
            Self::Qatari => "Qatar",
            Self::Singapore => "Singapore",
            Self::Tehran => "Institute of Geophysics, University of Tehran",
            Self::Turkey => "Diyanet, Turkey",
        }
    }

    /// Short tag used in logs and the tray tooltip.
    pub const fn short_name(self) -> &'static str {
        match self {
            Self::MuslimWorldLeague => "MWL",
            Self::Egyptian => "EGYPT",
            Self::Karachi => "KARACHI",
            Self::UmmAlQura => "MAKKAH",
            Self::Dubai => "DUBAI",
            Self::MoonsightingCommittee => "MOONSHOT",
            Self::NorthAmerica => "ISNA",
            Self::Kuwait => "KUWAIT",
            Self::Qatari => "QATAR",
            Self::Singapore => "SINGAPORE",
            Self::Tehran => "TEHRAN",
            Self::Turkey => "TURKEY",
        }
    }

    /// Translate an FFI/CLI integer back into a method, defaulting to MWL.
    pub fn from_id(id: i32) -> Self {
        Self::ALL
            .iter()
            .copied()
            .find(|m| *m as i32 == id)
            .unwrap_or(Self::MuslimWorldLeague)
    }

    /// The underlying adhan-crate method.
    pub(crate) fn adhan_method(self) -> adhan::Method {
        match self {
            Self::MuslimWorldLeague => adhan::Method::MuslimWorldLeague,
            Self::Egyptian => adhan::Method::Egyptian,
            Self::Karachi => adhan::Method::Karachi,
            Self::UmmAlQura => adhan::Method::UmmAlQura,
            Self::Dubai => adhan::Method::Dubai,
            Self::MoonsightingCommittee => adhan::Method::MoonsightingCommittee,
            Self::NorthAmerica => adhan::Method::NorthAmerica,
            Self::Kuwait => adhan::Method::Kuwait,
            Self::Qatari => adhan::Method::Qatar,
            Self::Singapore => adhan::Method::Singapore,
            Self::Tehran => adhan::Method::Tehran,
            Self::Turkey => adhan::Method::Turkey,
        }
    }
}

/// Which shadow length defines Asr. Discriminants are part of the FFI contract.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
#[repr(i32)]
pub enum AsrMadhab {
    #[default]
    Shafi = 0,
    Hanafi = 1,
}

impl AsrMadhab {
    pub fn from_id(id: i32) -> Self {
        if id == 1 { Self::Hanafi } else { Self::Shafi }
    }

    pub(crate) fn adhan_madhab(self) -> adhan::Madhab {
        match self {
            Self::Shafi => adhan::Madhab::Shafi,
            Self::Hanafi => adhan::Madhab::Hanafi,
        }
    }
}

/// How Fajr and Isha are bounded where twilight never ends. Discriminants are
/// part of the FFI contract.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
#[repr(i32)]
pub enum HighLatRule {
    /// Pick the rule adhan recommends for the latitude.
    #[default]
    Auto = 0,
    MiddleOfTheNight = 1,
    SeventhOfTheNight = 2,
    TwilightAngle = 3,
}

impl HighLatRule {
    pub fn from_id(id: i32) -> Self {
        match id {
            1 => Self::MiddleOfTheNight,
            2 => Self::SeventhOfTheNight,
            3 => Self::TwilightAngle,
            _ => Self::Auto,
        }
    }

    pub const fn display_name(self) -> &'static str {
        match self {
            Self::Auto => "Auto (recommended for latitude)",
            Self::MiddleOfTheNight => "Middle of the night",
            Self::SeventhOfTheNight => "Seventh of the night",
            Self::TwilightAngle => "Twilight angle",
        }
    }
}
