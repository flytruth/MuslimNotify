# Credits

## Adhan recordings (bundled)

The three bundled recordings are community-uploaded audio preserved on
archive.org. They are used here in the belief that they were shared freely
for this purpose. If you hold rights to a recording and object to its
inclusion, open an issue and it will be removed promptly.

| File                          | Length | Source                                                        |
| ----------------------------- | ------ | ------------------------------------------------------------- |
| `assets/adhan/adhan_mishary_style.mp3` | 3:40   | archive.org item `hamid_Azan` (`azan.mp3`)                  |
| `assets/adhan/adhan_abdulfattah.mp3`   | 2:26   | archive.org item `adhan-3abdlfatt7` (`adhan-3abdlfatt7.mp3`) |
| `assets/adhan/adhan_lms.mp3`           | 1:01   | archive.org item `lms-adhan-02` (`LMS-Adhan02.mp3`)          |

You can replace these with any recording via **Settings → Adhan audio →
Choose file…**.

## Prayer-time mathematics

Prayer times are computed by the [`adhan`](https://crates.io/crates/adhan)
Rust crate (© batoulapps and contributors, Apache-2.0/OR MIT dual licensing
applies to that crate), which is a port of the well-known batoulapps Adhan
reference algorithms. This project links it unmodified as a dependency of
`mn-core`.

## Typography

Arabic prayer names use your operating system's default Arabic fonts
(e.g. Noto Naskh Arabic, Segoe UI) — no fonts are bundled.
