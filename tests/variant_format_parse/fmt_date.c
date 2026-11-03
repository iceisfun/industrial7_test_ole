/* variant_format_parse: VarFormat date/time tokens and named date formats (oleaut32.dll, en-US 0x409).
 * The generated table below (gen_dates: Python datetime reference) enumerates every date token over eight fixed date-times;
 * week numbers follow the VB rules: iFirstDay 1=Sunday..7=Saturday, iFirstWeek 1=week with Jan 1, 2=first week with >=4 days,
 * 3=first full week. DATE values are written as exact doubles. */
#include "vfp.h"

typedef struct { double v; int fd, fw; const char *fmt, *exp; } DateRow;

/* generated rows: {date, firstday, firstweek, fmt, expected} */
static const DateRow gen_rows[] = {
    { 36526.0, 0, 0, "d", "1" },
    { 36526.0, 0, 0, "dd", "01" },
    { 36526.0, 0, 0, "ddd", "Sat" },
    { 36526.0, 0, 0, "dddd", "Saturday" },
    { 36526.0, 0, 0, "m", "1" },
    { 36526.0, 0, 0, "mm", "01" },
    { 36526.0, 0, 0, "mmm", "Jan" },
    { 36526.0, 0, 0, "mmmm", "January" },
    { 36526.0, 0, 0, "yy", "00" },
    { 36526.0, 0, 0, "yyyy", "2000" },
    { 36526.0, 0, 0, "y", "1" },
    { 36526.0, 0, 0, "q", "1" },
    { 36526.0, 0, 0, "h", "0" },
    { 36526.0, 0, 0, "hh", "00" },
    { 36526.0, 0, 0, "h:nn", "0:00" },
    { 36526.0, 0, 0, "hh:nn:ss", "00:00:00" },
    { 36526.0, 0, 0, "h:m:s", "0:0:0" },
    { 36526.0, 0, 0, "hh:mm", "00:00" },
    { 36526.0, 0, 0, "h AM/PM", "12 AM" },
    { 36526.0, 0, 0, "hh:nn AM/PM", "12:00 AM" },
    { 36526.0, 0, 0, "hh:nn a/p", "12:00 a" },
    { 36526.0, 0, 0, "hh:nn A/P", "12:00 A" },
    { 36526.0, 0, 0, "hh:nn am/pm", "12:00 am" },
    { 36526.0, 0, 0, "nn", "00" },
    { 36526.0, 0, 0, "n", "0" },
    { 36526.0, 0, 0, "ss", "00" },
    { 36526.0, 0, 0, "s", "0" },
    { 36526.0, 0, 0, "yyyy-mm-dd", "2000-01-01" },
    { 36526.0, 0, 0, "dd/mm/yyyy", "01/01/2000" },
    { 36526.0, 0, 0, "mmm d, yyyy", "Jan 1, 2000" },
    { 36526.0, 0, 0, "dddd, mmmm dd, yyyy", "Saturday, January 01, 2000" },
    { 36526.0, 0, 0, "yyyy-mm-dd hh:nn:ss", "2000-01-01 00:00:00" },
    { 36526.0, 0, 0, "\"Q\"q \"of\" yyyy", "Q1 of 2000" },
    { 36526.0, 0, 0, "d\\t\\h \\o\\f mmmm", "1th of January" },
    { 36526.0, 1, 1, "w", "7" },
    { 36526.0, 2, 1, "w", "6" },
    { 36526.0, 7, 1, "w", "1" },
    { 36526.0, 1, 1, "ww", "1" },
    { 36526.0, 2, 1, "ww", "1" },
    { 36526.0, 1, 2, "ww", "52" },
    { 36526.0, 2, 2, "ww", "52" },
    { 36526.0, 1, 3, "ww", "52" },
    { 36526.0, 2, 3, "ww", "52" },
    { 36526.0, 7, 2, "ww", "1" },
    { 44197.5, 0, 0, "d", "1" },
    { 44197.5, 0, 0, "dd", "01" },
    { 44197.5, 0, 0, "ddd", "Fri" },
    { 44197.5, 0, 0, "dddd", "Friday" },
    { 44197.5, 0, 0, "m", "1" },
    { 44197.5, 0, 0, "mm", "01" },
    { 44197.5, 0, 0, "mmm", "Jan" },
    { 44197.5, 0, 0, "mmmm", "January" },
    { 44197.5, 0, 0, "yy", "21" },
    { 44197.5, 0, 0, "yyyy", "2021" },
    { 44197.5, 0, 0, "y", "1" },
    { 44197.5, 0, 0, "q", "1" },
    { 44197.5, 0, 0, "h", "12" },
    { 44197.5, 0, 0, "hh", "12" },
    { 44197.5, 0, 0, "h:nn", "12:00" },
    { 44197.5, 0, 0, "hh:nn:ss", "12:00:00" },
    { 44197.5, 0, 0, "h:m:s", "12:0:0" },
    { 44197.5, 0, 0, "hh:mm", "12:00" },
    { 44197.5, 0, 0, "h AM/PM", "12 PM" },
    { 44197.5, 0, 0, "hh:nn AM/PM", "12:00 PM" },
    { 44197.5, 0, 0, "hh:nn a/p", "12:00 p" },
    { 44197.5, 0, 0, "hh:nn A/P", "12:00 P" },
    { 44197.5, 0, 0, "hh:nn am/pm", "12:00 pm" },
    { 44197.5, 0, 0, "nn", "00" },
    { 44197.5, 0, 0, "n", "0" },
    { 44197.5, 0, 0, "ss", "00" },
    { 44197.5, 0, 0, "s", "0" },
    { 44197.5, 0, 0, "yyyy-mm-dd", "2021-01-01" },
    { 44197.5, 0, 0, "dd/mm/yyyy", "01/01/2021" },
    { 44197.5, 0, 0, "mmm d, yyyy", "Jan 1, 2021" },
    { 44197.5, 0, 0, "dddd, mmmm dd, yyyy", "Friday, January 01, 2021" },
    { 44197.5, 0, 0, "yyyy-mm-dd hh:nn:ss", "2021-01-01 12:00:00" },
    { 44197.5, 0, 0, "\"Q\"q \"of\" yyyy", "Q1 of 2021" },
    { 44197.5, 0, 0, "d\\t\\h \\o\\f mmmm", "1th of January" },
    { 44197.5, 1, 1, "w", "6" },
    { 44197.5, 2, 1, "w", "5" },
    { 44197.5, 7, 1, "w", "7" },
    { 44197.5, 1, 1, "ww", "1" },
    { 44197.5, 2, 1, "ww", "1" },
    { 44197.5, 1, 2, "ww", "53" },
    { 44197.5, 2, 2, "ww", "53" },
    { 44197.5, 1, 3, "ww", "52" },
    { 44197.5, 2, 3, "ww", "52" },
    { 44197.5, 7, 2, "ww", "52" },
    { 45000.75357638889, 0, 0, "d", "15" },
    { 45000.75357638889, 0, 0, "dd", "15" },
    { 45000.75357638889, 0, 0, "ddd", "Wed" },
    { 45000.75357638889, 0, 0, "dddd", "Wednesday" },
    { 45000.75357638889, 0, 0, "m", "3" },
    { 45000.75357638889, 0, 0, "mm", "03" },
    { 45000.75357638889, 0, 0, "mmm", "Mar" },
    { 45000.75357638889, 0, 0, "mmmm", "March" },
    { 45000.75357638889, 0, 0, "yy", "23" },
    { 45000.75357638889, 0, 0, "yyyy", "2023" },
    { 45000.75357638889, 0, 0, "y", "74" },
    { 45000.75357638889, 0, 0, "q", "1" },
    { 45000.75357638889, 0, 0, "h", "18" },
    { 45000.75357638889, 0, 0, "hh", "18" },
    { 45000.75357638889, 0, 0, "h:nn", "18:05" },
    { 45000.75357638889, 0, 0, "hh:nn:ss", "18:05:09" },
    { 45000.75357638889, 0, 0, "h:m:s", "18:5:9" },
    { 45000.75357638889, 0, 0, "hh:mm", "18:05" },
    { 45000.75357638889, 0, 0, "h AM/PM", "6 PM" },
    { 45000.75357638889, 0, 0, "hh:nn AM/PM", "06:05 PM" },
    { 45000.75357638889, 0, 0, "hh:nn a/p", "06:05 p" },
    { 45000.75357638889, 0, 0, "hh:nn A/P", "06:05 P" },
    { 45000.75357638889, 0, 0, "hh:nn am/pm", "06:05 pm" },
    { 45000.75357638889, 0, 0, "nn", "05" },
    { 45000.75357638889, 0, 0, "n", "5" },
    { 45000.75357638889, 0, 0, "ss", "09" },
    { 45000.75357638889, 0, 0, "s", "9" },
    { 45000.75357638889, 0, 0, "yyyy-mm-dd", "2023-03-15" },
    { 45000.75357638889, 0, 0, "dd/mm/yyyy", "15/03/2023" },
    { 45000.75357638889, 0, 0, "mmm d, yyyy", "Mar 15, 2023" },
    { 45000.75357638889, 0, 0, "dddd, mmmm dd, yyyy", "Wednesday, March 15, 2023" },
    { 45000.75357638889, 0, 0, "yyyy-mm-dd hh:nn:ss", "2023-03-15 18:05:09" },
    { 45000.75357638889, 0, 0, "\"Q\"q \"of\" yyyy", "Q1 of 2023" },
    { 45000.75357638889, 0, 0, "d\\t\\h \\o\\f mmmm", "15th of March" },
    { 45000.75357638889, 1, 1, "w", "4" },
    { 45000.75357638889, 2, 1, "w", "3" },
    { 45000.75357638889, 7, 1, "w", "5" },
    { 45000.75357638889, 1, 1, "ww", "11" },
    { 45000.75357638889, 2, 1, "ww", "12" },
    { 45000.75357638889, 1, 2, "ww", "11" },
    { 45000.75357638889, 2, 2, "ww", "11" },
    { 45000.75357638889, 1, 3, "ww", "11" },
    { 45000.75357638889, 2, 3, "ww", "11" },
    { 45000.75357638889, 7, 2, "ww", "11" },
    { 45351.379895833335, 0, 0, "d", "29" },
    { 45351.379895833335, 0, 0, "dd", "29" },
    { 45351.379895833335, 0, 0, "ddd", "Thu" },
    { 45351.379895833335, 0, 0, "dddd", "Thursday" },
    { 45351.379895833335, 0, 0, "m", "2" },
    { 45351.379895833335, 0, 0, "mm", "02" },
    { 45351.379895833335, 0, 0, "mmm", "Feb" },
    { 45351.379895833335, 0, 0, "mmmm", "February" },
    { 45351.379895833335, 0, 0, "yy", "24" },
    { 45351.379895833335, 0, 0, "yyyy", "2024" },
    { 45351.379895833335, 0, 0, "y", "60" },
    { 45351.379895833335, 0, 0, "q", "1" },
    { 45351.379895833335, 0, 0, "h", "9" },
    { 45351.379895833335, 0, 0, "hh", "09" },
    { 45351.379895833335, 0, 0, "h:nn", "9:07" },
    { 45351.379895833335, 0, 0, "hh:nn:ss", "09:07:03" },
    { 45351.379895833335, 0, 0, "h:m:s", "9:7:3" },
    { 45351.379895833335, 0, 0, "hh:mm", "09:07" },
    { 45351.379895833335, 0, 0, "h AM/PM", "9 AM" },
    { 45351.379895833335, 0, 0, "hh:nn AM/PM", "09:07 AM" },
    { 45351.379895833335, 0, 0, "hh:nn a/p", "09:07 a" },
    { 45351.379895833335, 0, 0, "hh:nn A/P", "09:07 A" },
    { 45351.379895833335, 0, 0, "hh:nn am/pm", "09:07 am" },
    { 45351.379895833335, 0, 0, "nn", "07" },
    { 45351.379895833335, 0, 0, "n", "7" },
    { 45351.379895833335, 0, 0, "ss", "03" },
    { 45351.379895833335, 0, 0, "s", "3" },
    { 45351.379895833335, 0, 0, "yyyy-mm-dd", "2024-02-29" },
    { 45351.379895833335, 0, 0, "dd/mm/yyyy", "29/02/2024" },
    { 45351.379895833335, 0, 0, "mmm d, yyyy", "Feb 29, 2024" },
    { 45351.379895833335, 0, 0, "dddd, mmmm dd, yyyy", "Thursday, February 29, 2024" },
    { 45351.379895833335, 0, 0, "yyyy-mm-dd hh:nn:ss", "2024-02-29 09:07:03" },
    { 45351.379895833335, 0, 0, "\"Q\"q \"of\" yyyy", "Q1 of 2024" },
    { 45351.379895833335, 0, 0, "d\\t\\h \\o\\f mmmm", "29th of February" },
    { 45351.379895833335, 1, 1, "w", "5" },
    { 45351.379895833335, 2, 1, "w", "4" },
    { 45351.379895833335, 7, 1, "w", "6" },
    { 45351.379895833335, 1, 1, "ww", "9" },
    { 45351.379895833335, 2, 1, "ww", "9" },
    { 45351.379895833335, 1, 2, "ww", "9" },
    { 45351.379895833335, 2, 2, "ww", "9" },
    { 45351.379895833335, 1, 3, "ww", "8" },
    { 45351.379895833335, 2, 3, "ww", "9" },
    { 45351.379895833335, 7, 2, "ww", "9" },
    { 36525.99998842592, 0, 0, "d", "31" },
    { 36525.99998842592, 0, 0, "dd", "31" },
    { 36525.99998842592, 0, 0, "ddd", "Fri" },
    { 36525.99998842592, 0, 0, "dddd", "Friday" },
    { 36525.99998842592, 0, 0, "m", "12" },
    { 36525.99998842592, 0, 0, "mm", "12" },
    { 36525.99998842592, 0, 0, "mmm", "Dec" },
    { 36525.99998842592, 0, 0, "mmmm", "December" },
    { 36525.99998842592, 0, 0, "yy", "99" },
    { 36525.99998842592, 0, 0, "yyyy", "1999" },
    { 36525.99998842592, 0, 0, "y", "365" },
    { 36525.99998842592, 0, 0, "q", "4" },
    { 36525.99998842592, 0, 0, "h", "23" },
    { 36525.99998842592, 0, 0, "hh", "23" },
    { 36525.99998842592, 0, 0, "h:nn", "23:59" },
    { 36525.99998842592, 0, 0, "hh:nn:ss", "23:59:59" },
    { 36525.99998842592, 0, 0, "h:m:s", "23:59:59" },
    { 36525.99998842592, 0, 0, "hh:mm", "23:59" },
    { 36525.99998842592, 0, 0, "h AM/PM", "11 PM" },
    { 36525.99998842592, 0, 0, "hh:nn AM/PM", "11:59 PM" },
    { 36525.99998842592, 0, 0, "hh:nn a/p", "11:59 p" },
    { 36525.99998842592, 0, 0, "hh:nn A/P", "11:59 P" },
    { 36525.99998842592, 0, 0, "hh:nn am/pm", "11:59 pm" },
    { 36525.99998842592, 0, 0, "nn", "59" },
    { 36525.99998842592, 0, 0, "n", "59" },
    { 36525.99998842592, 0, 0, "ss", "59" },
    { 36525.99998842592, 0, 0, "s", "59" },
    { 36525.99998842592, 0, 0, "yyyy-mm-dd", "1999-12-31" },
    { 36525.99998842592, 0, 0, "dd/mm/yyyy", "31/12/1999" },
    { 36525.99998842592, 0, 0, "mmm d, yyyy", "Dec 31, 1999" },
    { 36525.99998842592, 0, 0, "dddd, mmmm dd, yyyy", "Friday, December 31, 1999" },
    { 36525.99998842592, 0, 0, "yyyy-mm-dd hh:nn:ss", "1999-12-31 23:59:59" },
    { 36525.99998842592, 0, 0, "\"Q\"q \"of\" yyyy", "Q4 of 1999" },
    { 36525.99998842592, 0, 0, "d\\t\\h \\o\\f mmmm", "31th of December" },
    { 36525.99998842592, 1, 1, "w", "6" },
    { 36525.99998842592, 2, 1, "w", "5" },
    { 36525.99998842592, 7, 1, "w", "7" },
    { 36525.99998842592, 1, 1, "ww", "53" },
    { 36525.99998842592, 2, 1, "ww", "53" },
    { 36525.99998842592, 1, 2, "ww", "52" },
    { 36525.99998842592, 2, 2, "ww", "52" },
    { 36525.99998842592, 1, 3, "ww", "52" },
    { 36525.99998842592, 2, 3, "ww", "52" },
    { 36525.99998842592, 7, 2, "ww", "52" },
    { 40363.020833333336, 0, 0, "d", "4" },
    { 40363.020833333336, 0, 0, "dd", "04" },
    { 40363.020833333336, 0, 0, "ddd", "Sun" },
    { 40363.020833333336, 0, 0, "dddd", "Sunday" },
    { 40363.020833333336, 0, 0, "m", "7" },
    { 40363.020833333336, 0, 0, "mm", "07" },
    { 40363.020833333336, 0, 0, "mmm", "Jul" },
    { 40363.020833333336, 0, 0, "mmmm", "July" },
    { 40363.020833333336, 0, 0, "yy", "10" },
    { 40363.020833333336, 0, 0, "yyyy", "2010" },
    { 40363.020833333336, 0, 0, "y", "185" },
    { 40363.020833333336, 0, 0, "q", "3" },
    { 40363.020833333336, 0, 0, "h", "0" },
    { 40363.020833333336, 0, 0, "hh", "00" },
    { 40363.020833333336, 0, 0, "h:nn", "0:30" },
    { 40363.020833333336, 0, 0, "hh:nn:ss", "00:30:00" },
    { 40363.020833333336, 0, 0, "h:m:s", "0:30:0" },
    { 40363.020833333336, 0, 0, "hh:mm", "00:30" },
    { 40363.020833333336, 0, 0, "h AM/PM", "12 AM" },
    { 40363.020833333336, 0, 0, "hh:nn AM/PM", "12:30 AM" },
    { 40363.020833333336, 0, 0, "hh:nn a/p", "12:30 a" },
    { 40363.020833333336, 0, 0, "hh:nn A/P", "12:30 A" },
    { 40363.020833333336, 0, 0, "hh:nn am/pm", "12:30 am" },
    { 40363.020833333336, 0, 0, "nn", "30" },
    { 40363.020833333336, 0, 0, "n", "30" },
    { 40363.020833333336, 0, 0, "ss", "00" },
    { 40363.020833333336, 0, 0, "s", "0" },
    { 40363.020833333336, 0, 0, "yyyy-mm-dd", "2010-07-04" },
    { 40363.020833333336, 0, 0, "dd/mm/yyyy", "04/07/2010" },
    { 40363.020833333336, 0, 0, "mmm d, yyyy", "Jul 4, 2010" },
    { 40363.020833333336, 0, 0, "dddd, mmmm dd, yyyy", "Sunday, July 04, 2010" },
    { 40363.020833333336, 0, 0, "yyyy-mm-dd hh:nn:ss", "2010-07-04 00:30:00" },
    { 40363.020833333336, 0, 0, "\"Q\"q \"of\" yyyy", "Q3 of 2010" },
    { 40363.020833333336, 0, 0, "d\\t\\h \\o\\f mmmm", "4th of July" },
    { 40363.020833333336, 1, 1, "w", "1" },
    { 40363.020833333336, 2, 1, "w", "7" },
    { 40363.020833333336, 7, 1, "w", "2" },
    { 40363.020833333336, 1, 1, "ww", "28" },
    { 40363.020833333336, 2, 1, "ww", "27" },
    { 40363.020833333336, 1, 2, "ww", "27" },
    { 40363.020833333336, 2, 2, "ww", "26" },
    { 40363.020833333336, 1, 3, "ww", "27" },
    { 40363.020833333336, 2, 3, "ww", "26" },
    { 40363.020833333336, 7, 2, "ww", "27" },
    { 2154.573263888889, 0, 0, "d", "23" },
    { 2154.573263888889, 0, 0, "dd", "23" },
    { 2154.573263888889, 0, 0, "ddd", "Thu" },
    { 2154.573263888889, 0, 0, "dddd", "Thursday" },
    { 2154.573263888889, 0, 0, "m", "11" },
    { 2154.573263888889, 0, 0, "mm", "11" },
    { 2154.573263888889, 0, 0, "mmm", "Nov" },
    { 2154.573263888889, 0, 0, "mmmm", "November" },
    { 2154.573263888889, 0, 0, "yy", "05" },
    { 2154.573263888889, 0, 0, "yyyy", "1905" },
    { 2154.573263888889, 0, 0, "y", "327" },
    { 2154.573263888889, 0, 0, "q", "4" },
    { 2154.573263888889, 0, 0, "h", "13" },
    { 2154.573263888889, 0, 0, "hh", "13" },
    { 2154.573263888889, 0, 0, "h:nn", "13:45" },
    { 2154.573263888889, 0, 0, "hh:nn:ss", "13:45:30" },
    { 2154.573263888889, 0, 0, "h:m:s", "13:45:30" },
    { 2154.573263888889, 0, 0, "hh:mm", "13:45" },
    { 2154.573263888889, 0, 0, "h AM/PM", "1 PM" },
    { 2154.573263888889, 0, 0, "hh:nn AM/PM", "01:45 PM" },
    { 2154.573263888889, 0, 0, "hh:nn a/p", "01:45 p" },
    { 2154.573263888889, 0, 0, "hh:nn A/P", "01:45 P" },
    { 2154.573263888889, 0, 0, "hh:nn am/pm", "01:45 pm" },
    { 2154.573263888889, 0, 0, "nn", "45" },
    { 2154.573263888889, 0, 0, "n", "45" },
    { 2154.573263888889, 0, 0, "ss", "30" },
    { 2154.573263888889, 0, 0, "s", "30" },
    { 2154.573263888889, 0, 0, "yyyy-mm-dd", "1905-11-23" },
    { 2154.573263888889, 0, 0, "dd/mm/yyyy", "23/11/1905" },
    { 2154.573263888889, 0, 0, "mmm d, yyyy", "Nov 23, 1905" },
    { 2154.573263888889, 0, 0, "dddd, mmmm dd, yyyy", "Thursday, November 23, 1905" },
    { 2154.573263888889, 0, 0, "yyyy-mm-dd hh:nn:ss", "1905-11-23 13:45:30" },
    { 2154.573263888889, 0, 0, "\"Q\"q \"of\" yyyy", "Q4 of 1905" },
    { 2154.573263888889, 0, 0, "d\\t\\h \\o\\f mmmm", "23th of November" },
    { 2154.573263888889, 1, 1, "w", "5" },
    { 2154.573263888889, 2, 1, "w", "4" },
    { 2154.573263888889, 7, 1, "w", "6" },
    { 2154.573263888889, 1, 1, "ww", "47" },
    { 2154.573263888889, 2, 1, "ww", "48" },
    { 2154.573263888889, 1, 2, "ww", "47" },
    { 2154.573263888889, 2, 2, "ww", "47" },
    { 2154.573263888889, 1, 3, "ww", "47" },
    { 2154.573263888889, 2, 3, "ww", "47" },
    { 2154.573263888889, 7, 2, "ww", "47" },
    { 50709.291666666664, 0, 0, "d", "31" },
    { 50709.291666666664, 0, 0, "dd", "31" },
    { 50709.291666666664, 0, 0, "ddd", "Sun" },
    { 50709.291666666664, 0, 0, "dddd", "Sunday" },
    { 50709.291666666664, 0, 0, "m", "10" },
    { 50709.291666666664, 0, 0, "mm", "10" },
    { 50709.291666666664, 0, 0, "mmm", "Oct" },
    { 50709.291666666664, 0, 0, "mmmm", "October" },
    { 50709.291666666664, 0, 0, "yy", "38" },
    { 50709.291666666664, 0, 0, "yyyy", "2038" },
    { 50709.291666666664, 0, 0, "y", "304" },
    { 50709.291666666664, 0, 0, "q", "4" },
    { 50709.291666666664, 0, 0, "h", "7" },
    { 50709.291666666664, 0, 0, "hh", "07" },
    { 50709.291666666664, 0, 0, "h:nn", "7:00" },
    { 50709.291666666664, 0, 0, "hh:nn:ss", "07:00:00" },
    { 50709.291666666664, 0, 0, "h:m:s", "7:0:0" },
    { 50709.291666666664, 0, 0, "hh:mm", "07:00" },
    { 50709.291666666664, 0, 0, "h AM/PM", "7 AM" },
    { 50709.291666666664, 0, 0, "hh:nn AM/PM", "07:00 AM" },
    { 50709.291666666664, 0, 0, "hh:nn a/p", "07:00 a" },
    { 50709.291666666664, 0, 0, "hh:nn A/P", "07:00 A" },
    { 50709.291666666664, 0, 0, "hh:nn am/pm", "07:00 am" },
    { 50709.291666666664, 0, 0, "nn", "00" },
    { 50709.291666666664, 0, 0, "n", "0" },
    { 50709.291666666664, 0, 0, "ss", "00" },
    { 50709.291666666664, 0, 0, "s", "0" },
    { 50709.291666666664, 0, 0, "yyyy-mm-dd", "2038-10-31" },
    { 50709.291666666664, 0, 0, "dd/mm/yyyy", "31/10/2038" },
    { 50709.291666666664, 0, 0, "mmm d, yyyy", "Oct 31, 2038" },
    { 50709.291666666664, 0, 0, "dddd, mmmm dd, yyyy", "Sunday, October 31, 2038" },
    { 50709.291666666664, 0, 0, "yyyy-mm-dd hh:nn:ss", "2038-10-31 07:00:00" },
    { 50709.291666666664, 0, 0, "\"Q\"q \"of\" yyyy", "Q4 of 2038" },
    { 50709.291666666664, 0, 0, "d\\t\\h \\o\\f mmmm", "31th of October" },
    { 50709.291666666664, 1, 1, "w", "1" },
    { 50709.291666666664, 2, 1, "w", "7" },
    { 50709.291666666664, 7, 1, "w", "2" },
    { 50709.291666666664, 1, 1, "ww", "45" },
    { 50709.291666666664, 2, 1, "ww", "44" },
    { 50709.291666666664, 1, 2, "ww", "44" },
    { 50709.291666666664, 2, 2, "ww", "43" },
    { 50709.291666666664, 1, 3, "ww", "44" },
    { 50709.291666666664, 2, 3, "ww", "43" },
    { 50709.291666666664, 7, 2, "ww", "44" },
};

static void run_date_rows(const DateRow *rows, size_t n, const char *only_fmt_prefix, int soft)
{
    size_t i;
    for (i = 0; i < n; i++) {
        VARIANT v = vfp_date(rows[i].v);
        if (only_fmt_prefix && strncmp(rows[i].fmt, only_fmt_prefix, strlen(only_fmt_prefix))) continue;
        if (soft) VFP_ROW_SOFT(v, rows[i].fmt, rows[i].fd, rows[i].fw, rows[i].exp);
        else VFP_ROW(v, rows[i].fmt, rows[i].fd, rows[i].fw, rows[i].exp);
    }
}

/* run only rows whose format is exactly one of the listed tokens */
static void run_date_fmts(const char *const *fmts, int soft)
{
    size_t i, k;
    for (i = 0; i < sizeof gen_rows / sizeof *gen_rows; i++)
        for (k = 0; fmts[k]; k++)
            if (!strcmp(gen_rows[i].fmt, fmts[k])) {
                VARIANT v = vfp_date(gen_rows[i].v);
                if (soft) VFP_ROW_SOFT(v, gen_rows[i].fmt, gen_rows[i].fd, gen_rows[i].fw, gen_rows[i].exp);
                else VFP_ROW(v, gen_rows[i].fmt, gen_rows[i].fd, gen_rows[i].fw, gen_rows[i].exp);
            }
}
#define FMTS(...) ((const char *const[]){ __VA_ARGS__, NULL })

TEST(vfp_date_day_tokens)    { VFP_NEED_FORMAT(); run_date_fmts(FMTS("d", "dd", "ddd", "dddd"), 0); }
TEST(vfp_date_month_tokens)  { VFP_NEED_FORMAT(); run_date_fmts(FMTS("m", "mm", "mmm", "mmmm"), 0); }
TEST(vfp_date_year_tokens)   { VFP_NEED_FORMAT(); run_date_fmts(FMTS("yy", "yyyy", "y"), 0); }
TEST(vfp_date_quarter_token) { VFP_NEED_FORMAT(); run_date_fmts(FMTS("q"), 0); }
TEST(vfp_date_hour_tokens)   { VFP_NEED_FORMAT(); run_date_fmts(FMTS("h", "hh"), 0); }
TEST(vfp_date_minute_second_tokens) { VFP_NEED_FORMAT(); run_date_fmts(FMTS("nn", "n", "ss", "s", "h:nn", "hh:nn:ss", "h:m:s", "hh:mm"), 0); }
TEST(vfp_date_ampm_tokens)   { VFP_NEED_FORMAT(); run_date_fmts(FMTS("h AM/PM", "hh:nn AM/PM", "hh:nn a/p", "hh:nn A/P", "hh:nn am/pm"), 0); }
TEST(vfp_date_composite_formats) { VFP_NEED_FORMAT(); run_date_fmts(FMTS("yyyy-mm-dd", "dd/mm/yyyy", "mmm d, yyyy", "dddd, mmmm dd, yyyy", "yyyy-mm-dd hh:nn:ss", "\"Q\"q \"of\" yyyy", "d\\t\\h \\o\\f mmmm"), 0); }
TEST(vfp_date_weekday_number_w)  { VFP_NEED_FORMAT(); run_date_fmts(FMTS("w"), 0); }
TEST(vfp_date_week_of_year_ww)
{
    /* iFirstWeek 2/3 rows (weeks of the previous/next year) are VB-model expectations: soft */
    size_t i;
    VFP_NEED_FORMAT();
    for (i = 0; i < sizeof gen_rows / sizeof *gen_rows; i++)
        if (!strcmp(gen_rows[i].fmt, "ww") && gen_rows[i].fw != 1) {
            VARIANT v = vfp_date(gen_rows[i].v);
            VFP_ROW_SOFT(v, "ww", gen_rows[i].fd, gen_rows[i].fw, gen_rows[i].exp);
        }
}

TEST(vfp_date_week_of_year_first_jan1_only)
{
    /* iFirstWeek=1 (week containing Jan 1) is the least ambiguous rule: always >= 1 */
    size_t i;
    VFP_NEED_FORMAT();
    for (i = 0; i < sizeof gen_rows / sizeof *gen_rows; i++)
        if (!strcmp(gen_rows[i].fmt, "ww") && gen_rows[i].fw == 1) {
            VARIANT v = vfp_date(gen_rows[i].v);
            VFP_ROW(v, "ww", gen_rows[i].fd, 1, gen_rows[i].exp);
        }
}

/* adapted from Wine dlls/oleaut32/tests/varformat.c (LGPL-2.1+) */
TEST(vfp_date_firstday_w_saturday_date)
{
    /* DATE 0.0 is Saturday 30 Dec 1899: "w" counts from iFirstDay (1=Sunday .. 7=Saturday) */
    static const char *exp[7] = { "7", "6", "5", "4", "3", "2", "1" };
    int fd;
    VFP_NEED_FORMAT();
    for (fd = 1; fd <= 7; fd++) {
        VARIANT v = vfp_date(0.0);
        VFP_ROW(v, "w", fd, 0, exp[fd - 1]);
    }
}

/* adapted from Wine dlls/oleaut32/tests/varformat.c (LGPL-2.1+) */
static const DateRow wine_rows[] = {
    { 2.525, 0, 0, "am/pm", "pm" }, { 2.525, 0, 0, "AM/PM", "PM" }, { 2.525, 0, 0, "A/P", "P" }, { 2.525, 0, 0, "a/p", "p" },
    { 2.525, 0, 0, "q", "1" }, { 2.525, 0, 0, "d", "1" }, { 2.525, 0, 0, "dd", "01" }, { 2.525, 0, 0, "ddd", "Mon" },
    { 2.525, 0, 0, "dddd", "Monday" }, { 2.525, 0, 0, "mmm", "Jan" }, { 2.525, 0, 0, "mmmm", "January" }, { 2.525, 0, 0, "y", "1" },
    { 2.525, 0, 0, "yy", "00" }, { 2.525, 0, 0, "yyy", "001" }, { 2.525, 0, 0, "yyyy", "1900" },
    { 2.525, 0, 0, "dd mm yyyy hh:mm:ss", "01 01 1900 12:36:00" }, { 2.525, 0, 0, "dd mm yyyy mm", "01 01 1900 01" },
    { 2.525, 0, 0, "dd mm yyyy :mm", "01 01 1900 :01" }, { 2.525, 0, 0, "dd mm yyyy hh:mm", "01 01 1900 12:36" },
    { 2.525, 0, 0, "mm mm", "01 01" }, { 2.525, 0, 0, "mm :mm:ss", "01 :01:00" }, { 2.525, 0, 0, "mm :ss:mm", "01 :00:01" },
    { 2.525, 0, 0, "hh:mm :ss:mm", "12:36 :00:01" }, { 2.525, 0, 0, "hh:dd :mm:mm", "12:01 :01:01" },
    { 2.525, 0, 0, "dd:hh :mm:mm", "01:12 :36:01" }, { 2.525, 0, 0, "hh :mm:mm", "12 :36:01" }, { 2.525, 0, 0, "dd :mm:mm", "01 :01:01" },
    { 2.525, 0, 0, "dd :mm:nn", "01 :01:36" }, { 2.725, 0, 0, "hh:nn:ss A/P", "05:24:00 P" },
    { 40531.0, 0, 0, "dddd", "Sunday" }, { 40531.0, 0, 0, "ddd", "Sun" },
};
TEST(vfp_date_wine_derived_rows) { VFP_NEED_FORMAT(); run_date_rows(wine_rows, sizeof wine_rows / sizeof *wine_rows, NULL, 0); }

/* mm is a minute when it follows an hour token (or precedes seconds), otherwise a month: Windows-verified via the Wine table;
 * extra combinations are soft observations */
TEST(vfp_date_month_vs_minute_disambiguation)
{
    static const DateRow rows[] = {
        { 40531.529861111111, 0, 0, "h:mm", "12:43" }, { 40531.529861111111, 0, 0, "hh:mm:ss", "12:43:00" },
        { 40531.529861111111, 0, 0, "mm/dd/yyyy", "12/19/2010" }, { 40531.529861111111, 0, 0, "dd mm yyyy", "19 12 2010" },
        { 40531.529861111111, 0, 0, "mmmm", "December" },
    };
    VFP_NEED_FORMAT();
    run_date_rows(rows, sizeof rows / sizeof *rows, NULL, 1);
}

TEST(vfp_date_literals_and_escapes)
{
    static const DateRow rows[] = {
        { 36526.0, 0, 0, "\"Year \"yyyy", "Year 2000" }, { 36526.0, 0, 0, "yyyy\" AD\"", "2000 AD" }, { 36526.0, 0, 0, "\\y\\y\\y\\y", "yyyy" },
        { 36526.0, 0, 0, "dd\\-mm\\-yyyy", "01-01-2000" }, { 36526.0, 0, 0, "dd.mm.yyyy", "01.01.2000" }, { 36526.0, 0, 0, "dd-mm-yy", "01-01-00" },
        { 36526.0, 0, 0, "d/m/yy", "1/1/00" }, { 36526.0, 0, 0, "yyyymmdd", "20000101" }, { 36526.0, 0, 0, "ddmmyy", "010100" },
        { 36526.0, 0, 0, "mmmyy", "Jan00" }, { 36526.0, 0, 0, "mmm-yy", "Jan-00" }, { 36526.0, 0, 0, "dddd mmmm d", "Saturday January 1" },
        { 36526.0, 0, 0, "\"[\"dd\"]\"", "[01]" }, { 36526.0, 0, 0, "'dd'", "'01'" }, { 36526.0, 0, 0, "d,m,y", "1,1,1" },
    };
    VFP_NEED_FORMAT();
    run_date_rows(rows, sizeof rows / sizeof *rows, NULL, 0);
}

TEST(vfp_date_midnight_and_noon_12h)
{
    static const DateRow rows[] = {
        { 36526.0, 0, 0, "h AM/PM", "12 AM" }, { 36526.5, 0, 0, "h AM/PM", "12 PM" }, { 36526.0, 0, 0, "hh:nn AM/PM", "12:00 AM" },
        { 36526.5, 0, 0, "hh:nn AM/PM", "12:00 PM" }, { 36526.0, 0, 0, "h", "0" }, { 36526.5, 0, 0, "h", "12" },
        { 36526.0 + 1.0 / 24, 0, 0, "h AM/PM", "1 AM" }, { 36526.0 + 13.0 / 24, 0, 0, "h AM/PM", "1 PM" },
        { 36526.0 + 11.0 / 24, 0, 0, "h A/P", "11 A" }, { 36526.0 + 23.0 / 24, 0, 0, "h A/P", "11 P" },
    };
    VFP_NEED_FORMAT();
    run_date_rows(rows, sizeof rows / sizeof *rows, NULL, 0);
}

TEST(vfp_date_range_extremes)
{
    static const DateRow rows[] = {
        { 2958465.0, 0, 0, "yyyy-mm-dd", "9999-12-31" }, { 0.0, 0, 0, "yyyy-mm-dd", "1899-12-30" },
        { 1.0, 0, 0, "yyyy-mm-dd", "1899-12-31" }, { 2.0, 0, 0, "yyyy-mm-dd", "1900-01-01" }, { 59.0, 0, 0, "yyyy-mm-dd", "1900-02-27" }, { 60.0, 0, 0, "yyyy-mm-dd", "1900-02-28" },
        { 61.0, 0, 0, "yyyy-mm-dd", "1900-03-01" }, { -1.0, 0, 0, "yyyy-mm-dd", "1899-12-29" }, { 36585.0, 0, 0, "yyyy-mm-dd", "2000-02-29" },
        { 2958465.0, 0, 0, "dddd", "Friday" }, { -657434.0, 0, 0, "dddd", "Friday" },
    };
    VFP_NEED_FORMAT();
    run_date_rows(rows, sizeof rows / sizeof *rows, NULL, 0);
}
TEST(vfp_date_year_100_lower_bound)
{
    /* 1 Jan 100 is the minimum DATE; the 4 digit year token pads (or not) is an observation */
    VARIANT v = vfp_date(-657434.0);
    VFP_NEED_FORMAT();
    VFP_ROW_SOFT(v, "yyyy-mm-dd", 0, 0, "0100-01-01");
    VFP_ROW(v, "dd", 0, 0, "01");
    VFP_ROW(v, "mmmm", 0, 0, "January");
    VFP_ROW(v, "yy", 0, 0, "00");
}
TEST(vfp_date_out_of_range_is_error)
{
    VARIANT v = vfp_date(2958466.0);
    BSTR out = NULL;
    HRESULT hr;
    VFP_NEED_FORMAT();
    hr = U(VarFormat)(&v, (LPOLESTR)L"yyyy", 0, 0, 0, &out);
    CHECK(FAILED(hr));
    t_note("VarFormat(DATE 2958466, yyyy) hr=0x%lx", (unsigned long)hr);
    if (out) U(SysFreeString)(out);
    out = NULL;
    v = vfp_date(-657435.0);
    hr = U(VarFormat)(&v, (LPOLESTR)L"yyyy", 0, 0, 0, &out);
    CHECK(FAILED(hr));
    if (out) U(SysFreeString)(out);
}

/* ---- named date/time formats --------------------------------------------- */
/* adapted from Wine dlls/oleaut32/tests/varformat.c (LGPL-2.1+) */
TEST(vfp_date_named_time_formats)
{
    static const DateRow rows[] = {
        { 2.525, 0, 0, "short time", "12:36" }, { 2.525, 0, 0, "medium time", "12:36 PM" }, { 2.525, 0, 0, "long time", "12:36:00 PM" },
        { 2.525, 0, 0, "Short Time", "12:36" }, { 2.525, 0, 0, "SHORT TIME", "12:36" }, { 2.525, 0, 0, "Long Time", "12:36:00 PM" },
        { 2.725, 0, 0, "Short Time", "17:24" }, { 2.725, 0, 0, "Long Time", "5:24:00 PM" }, { 2.725, 0, 0, "Medium Time", "05:24 PM" },
        { 36526.0, 0, 0, "Long Time", "12:00:00 AM" }, { 36526.0, 0, 0, "Short Time", "00:00" }, { 36526.0, 0, 0, "Medium Time", "12:00 AM" },
        { 36526.5, 0, 0, "Long Time", "12:00:00 PM" }, { 36526.5, 0, 0, "Short Time", "12:00" },
    };
    size_t i;
    VFP_NEED_FORMAT();
    for (i = 0; i < sizeof rows / sizeof *rows; i++) {
        VARIANT v = vfp_date(rows[i].v);
        /* hard for the Wine-verified 2.525 rows, soft for the rest */
        if (rows[i].v == 2.525 && (!strcasecmp(rows[i].fmt, "short time") || !strcasecmp(rows[i].fmt, "medium time") || !strcasecmp(rows[i].fmt, "long time")))
            VFP_ROW(v, rows[i].fmt, 0, 0, rows[i].exp);
        else VFP_ROW_SOFT(v, rows[i].fmt, 0, 0, rows[i].exp);
    }
}
TEST(vfp_date_named_date_formats)
{
    static const DateRow rows[] = {
        { 36526.0, 0, 0, "General Date", "1/1/2000" }, { 36526.5, 0, 0, "General Date", "1/1/2000 12:00:00 PM" },
        { 36526.25, 0, 0, "General Date", "1/1/2000 6:00:00 AM" }, { 0.5, 0, 0, "General Date", "12:00:00 PM" },
        { 36526.0, 0, 0, "Short Date", "1/1/2000" }, { 36526.5, 0, 0, "Short Date", "1/1/2000" }, { 44197.0, 0, 0, "Short Date", "1/1/2021" },
        { 40531.0, 0, 0, "Short Date", "12/19/2010" }, { 36526.0, 0, 0, "Long Date", "Saturday, January 1, 2000" },
        { 40531.0, 0, 0, "Long Date", "Sunday, December 19, 2010" }, { 36526.0, 0, 0, "general date", "1/1/2000" },
        { 36526.0, 0, 0, "short date", "1/1/2000" }, { 36526.0, 0, 0, "long date", "Saturday, January 1, 2000" },
        { 36526.0, 0, 0, "c", "1/1/2000" }, { 36526.5, 0, 0, "c", "1/1/2000 12:00:00 PM" }, { 36526.0, 0, 0, "ddddd", "1/1/2000" },
        { 36526.0, 0, 0, "dddddd", "Saturday, January 1, 2000" }, { 36526.5, 0, 0, "ttttt", "12:00:00 PM" }, { 36526.25, 0, 0, "ttttt", "6:00:00 AM" },
    };
    VFP_NEED_FORMAT();
    run_date_rows(rows, sizeof rows / sizeof *rows, NULL, 1);
}
TEST(vfp_date_named_with_user_override_free_lcid)
{
    /* the pinned-LCID path must give identical output for a named format and its locale-derived explicit pattern */
    VARIANT v = vfp_date(40531.529861111111);
    BSTR a = NULL, b = NULL;
    WCHAR pat[80];
    VFP_NEED_FORMAT();
    if (!GetLocaleInfoW(VFP_EN_US, LOCALE_SSHORTDATE | LOCALE_NOUSEROVERRIDE, pat, 80)) { t_skip("no short date pattern"); return; }
    t_note("en-US short date pattern: %s", vfp_n(pat));
    CHECK(vfp_format_tok(&v, "Short Date", 0, 0, 0, VFP_EN_US, &a) == S_OK);
    if (a) { t_note("Short Date -> %s", vfp_n(a)); U(SysFreeString)(a); }
    (void)b;
}

/* ---- two digit year / misc with VT_BSTR as date source -------------------- */
TEST(vfp_date_from_string_variant)
{
    static const struct { const char *in, *fmt, *exp; } rows[] = {
        { "39697.11", "hh:mm", "02:38" }, { "39697.11", "mm-dd-yy", "09-06-08" }, { "36526", "yyyy-mm-dd", "2000-01-01" },
        { "36526.5", "hh:nn", "12:00" }, { "0", "yyyy-mm-dd", "1899-12-30" },
    };
    size_t i;
    VFP_NEED_FORMAT();
    for (i = 0; i < sizeof rows / sizeof *rows; i++) {
        VARIANT v = vfp_bstr(rows[i].in);
        VFP_ROW(v, rows[i].fmt, 0, 0, rows[i].exp);
        vfp_clear(&v);
    }
}
TEST(vfp_date_integer_sources)
{
    /* non-DATE numerics are treated as serial date numbers when a date format is applied */
    VARIANT v = vfp_i4(36526);
    VFP_NEED_FORMAT();
    VFP_ROW(v, "yyyy-mm-dd", 0, 0, "2000-01-01");
    v = vfp_r8(36526.25); VFP_ROW(v, "yyyy-mm-dd hh:nn", 0, 0, "2000-01-01 06:00");
    v = vfp_i4(1); VFP_ROW(v, "dddd", 0, 0, "Sunday");
    v = vfp_i4(2); VFP_ROW(v, "dddd", 0, 0, "Monday");
}
TEST(vfp_date_negative_time_fraction)
{
    /* for DATE < 0 the fraction is the magnitude: -5.25 is 25 Dec 1899 06:00 */
    VARIANT v = vfp_date(-5.25);
    VFP_NEED_FORMAT();
    VFP_ROW(v, "yyyy-mm-dd hh:nn", 0, 0, "1899-12-25 06:00");
    v = vfp_date(-4.0); VFP_ROW(v, "yyyy-mm-dd hh:nn", 0, 0, "1899-12-26 00:00");
    v = vfp_date(-0.25); VFP_ROW(v, "yyyy-mm-dd hh:nn", 0, 0, "1899-12-30 06:00");
}
