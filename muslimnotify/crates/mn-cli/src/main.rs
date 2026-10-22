//! mn-cli — headless MuslimNotify client.
//!
//! Prints the prayer table for a location/date. Doubles as the CI
//! verification vehicle for the Rust core: every feature the GUI uses is
//! exercisable without a display server.
//!
//! Exit codes: 0 success, 1 bad usage, 2 computation error.

use mn_core::model::{AsrMadhab, CalcMethod, HighLatRule, PRAYER_COUNT};
use mn_core::prayer_index;
use mn_core::{compute_local_times, format_daily, format_clock};

struct Args {
    lat: f64,
    lng: f64,
    method: i32,
    madhab: i32,
    high_lat: i32,
    tz: Option<i64>,
    date: Option<(i32, u32, u32)>,
    use_24h: bool,
    list_methods: bool,
    city: Option<String>,
}

fn print_usage() -> ! {
    println!("MuslimNotify CLI (core v{})", mn_core_version());
    println!();
    println!("USAGE:");
    println!("  mn-cli --lat <f64> --lng <f64> [options]");
    println!();
    println!("OPTIONS:");
    println!("  --lat <deg>          Latitude, north positive");
    println!("  --lng <deg>          Longitude, east positive");
    println!("  --method <id>        Calculation method id, 0-11 (default 0 = MWL)");
    println!("  --madhab <id>        0 = Shafi (default), 1 = Hanafi (Asr)");
    println!("  --high-lat <id>      0 = Auto (default), 1 = Middle, 2 = Seventh, 3 = Twilight");
    println!("  --tz <secs>          UTC offset seconds east; omit to guess from longitude");
    println!("  --date <YYYY-MM-DD>  Civil date; omit for today (UTC)");
    println!("  --city <name>        Free-form label shown in the output header");
    println!("  --12h                12-hour clock (default 24h)");
    println!("  --list-methods       Print the method table and exit");
    println!();
    println!("EXAMPLES:");
    println!("  mn-cli --lat 21.4225 --lng 39.8262 --method 3 --city Makkah");
    println!("  mn-cli --lat 40.7128 --lng -74.0060 --method 6 --tz -14400 --12h");
    std::process::exit(0);
}

/// Rust core version via the same string the FFI exposes.
fn mn_core_version() -> &'static str {
    env!("CARGO_PKG_VERSION")
}

fn parse_args() -> Args {
    let raw: Vec<String> = std::env::args().skip(1).collect();
    if raw.is_empty() {
        print_usage();
    }
    let mut a = Args {
        lat: f64::NAN,
        lng: f64::NAN,
        method: 0,
        madhab: 0,
        high_lat: 0,
        tz: None,
        date: None,
        use_24h: true,
        list_methods: false,
        city: None,
    };
    let mut it = raw.iter();
    while let Some(flag) = it.next() {
        let flag = flag.as_str();
        match flag {
            "--lat" => a.lat = take_val(&mut it, flag).parse().unwrap_or_else(|_| die("--lat")),
            "--lng" => a.lng = take_val(&mut it, flag).parse().unwrap_or_else(|_| die("--lng")),
            "--method" => a.method = take_val(&mut it, flag).parse().unwrap_or_else(|_| die("--method")),
            "--madhab" => a.madhab = take_val(&mut it, flag).parse().unwrap_or_else(|_| die("--madhab")),
            "--high-lat" => a.high_lat = take_val(&mut it, flag).parse().unwrap_or_else(|_| die("--high-lat")),
            "--tz" => a.tz = Some(take_val(&mut it, flag).parse().unwrap_or_else(|_| die("--tz"))),
            "--date" => {
                let s = take_val(&mut it, flag);
                let parts: Vec<&str> = s.split('-').collect();
                if parts.len() != 3 {
                    die("--date expects YYYY-MM-DD");
                }
                let y: i32 = parts[0].parse().unwrap_or_else(|_| die("--date year"));
                let m: u32 = parts[1].parse().unwrap_or_else(|_| die("--date month"));
                let d: u32 = parts[2].parse().unwrap_or_else(|_| die("--date day"));
                a.date = Some((y, m, d));
            }
            "--city" => a.city = Some(take_val(&mut it, flag).to_string()),
            "--12h" => a.use_24h = false,
            "--list-methods" => a.list_methods = true,
            "--help" | "-h" => print_usage(),
            other => {
                eprintln!("error: unknown flag {other}");
                std::process::exit(1);
            }
        }
    }
    a
}

fn die(flag: &str) -> ! {
    eprintln!("error: bad value for {flag}");
    std::process::exit(1);
}

/// Pull the next argument as a flag value, exiting with a message if absent.
fn take_val<'a>(it: &mut std::slice::Iter<'a, String>, flag: &str) -> &'a String {
    it.next().unwrap_or_else(|| {
        eprintln!("error: {flag} needs a value");
        std::process::exit(1);
    })
}

fn main() {
    let args = parse_args();

    if args.list_methods {
        println!("id  tag        authority");
        for m in CalcMethod::ALL {
            println!("{:<2}  {:<10} {}", m as i32, m.short_name(), m.display_name());
        }
        return;
    }

    if args.lat.is_nan() || args.lng.is_nan() {
        eprintln!("error: --lat and --lng are required");
        std::process::exit(1);
    }

    // Date: today by UTC calendar (the CLI has no OS tz dependency).
    let (y, m, d) = args.date.unwrap_or_else(today_utc);
    // Timezone: caller-supplied, else a longitude-based guess (~1h per 15°).
    let tz = args.tz.unwrap_or_else(|| (args.lng / 15.0).round() as i64 * 3600);

    let method = CalcMethod::from_id(args.method);
    let times = compute_local_times(
        y, m, d, tz, args.lat, args.lng, method,
        AsrMadhab::from_id(args.madhab), HighLatRule::from_id(args.high_lat),
    );

    let times = match times {
        Ok(t) => t,
        Err(e) => {
            eprintln!("error: {e:?}");
            std::process::exit(2);
        }
    };

    let formatted = format_daily(&times, args.use_24h);
    let tz_h = tz as f64 / 3600.0;
    match &args.city {
        Some(c) => println!("MuslimNotify — {c}"),
        None => println!("MuslimNotify"),
    }
    println!("Coords: ({:.4}, {:.4})  UTC{:+.1}  Method: {}", args.lat, args.lng, tz_h, method.short_name());
    println!("Date: {y:04}-{m:02}-{d:02}");
    println!();
    println!("{:<10} {:<10} {}", "Prayer", "Time", "Arabic");
    for i in 0..PRAYER_COUNT {
        let marker = if i == prayer_index::SUNRISE { " (info)" } else { "" };
        println!("{:<10} {:<10} {}{}", mn_core::PRAYER_NAMES_EN[i], formatted[i], mn_core::PRAYER_NAMES_AR[i], marker);
    }
    println!();

    // Current/next summary at the time of printing.
    let now_secs = args
        .date
        .map(|_| 0_i64)
        .unwrap_or_else(now_secs_since_utc_midnight);
    match times.next_at(now_secs) {
        Some(idx) => {
            let t = times.times[idx].unwrap();
            println!("Next: {} at {}", mn_core::PRAYER_NAMES_EN[idx], format_clock(t, args.use_24h));
        }
        None => println!("Next: Isha has passed — Fajr of tomorrow (rerun with tomorrow's date)"),
    }
}

/// Today's UTC civil date, dependency-free.
fn today_utc() -> (i32, u32, u32) {
    let now = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .expect("clock before 1970");
    let days = now.as_secs() as i64 / 86_400;
    // Reuse the engine's civil conversion through a tiny helper.
    days_to_civil(days)
}

fn days_to_civil(days: i64) -> (i32, u32, u32) {
    let z = days + 719_468;
    let era = if z >= 0 { z } else { z - 146_096 } / 146_097;
    let doe = z - era * 146_097;
    let yoe = (doe - doe / 1460 + doe / 36_524 - doe / 146_096) / 365;
    let y = yoe + era * 400;
    let doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    let mp = (5 * doy + 2) / 153;
    let d = doy - (153 * mp + 2) / 5 + 1;
    let m = if mp < 10 { mp + 3 } else { mp - 9 };
    let y = if m <= 2 { y + 1 } else { y };
    (y as i32, m as u32, d as u32)
}

fn now_secs_since_utc_midnight() -> i64 {
    let now = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .expect("clock before 1970");
    (now.as_secs() % 86_400) as i64
}
