#include "DateTime.h"
#include <ctime>
#include <cstring>
#include <string>
#include <cstdio>
#include "esp_log.h"
#include <limits>

// --- Constructors ---

DateTime::DateTime(std::time_t utc) : utc(utc) {}

DateTime::DateTime() : utc(0) {}

// --- Factory Methods ---

DateTime DateTime::Now() {
    return DateTime(time(nullptr));
}

DateTime DateTime::FromUtc(std::time_t secondsSinceEpoch) {
    return DateTime(secondsSinceEpoch);
}


DateTime DateTime::FromLocal(const std::tm& localTm) {
    std::tm tmp = localTm;
    return DateTime(mktime(&tmp));
}

bool DateTime::FromStringLocal(DateTime& result, const char* str, const char* format) {
    std::tm t = {};
    if (strptime(str, format, &t) == nullptr) {
        ESP_LOGE(TAG, "Couln't convert to datetime '%s', format '%s'", str, format );
        return false;
    }
    result = DateTime(mktime(&t));
    return true;
}

DateTime DateTime::MinValue()
{
    return DateTime(std::numeric_limits<std::time_t>::min());
}

DateTime DateTime::MaxValue()
{
    return DateTime(std::numeric_limits<std::time_t>::max());
}

#ifdef TIMEGM_FUNCTION
DateTime DateTime::FromUtc(const std::tm& utcTm) {
    std::tm tmp = utcTm;  // timegm() may modify the tm struct
    return DateTime(TIMEGM_FUNCTION(&tmp));
}

DateTime DateTime::FromStringUtc(const std::string& str, const std::string& format) {
    std::tm t = {};
    if (strptime(str.c_str(), format.c_str(), &t) == nullptr) {
        // Handle parse error as needed; here we return a default DateTime.
        return DateTime();
    }
    return DateTime(TIMEGM_FUNCTION(&t));
}
#endif


// --- String Conversion ---

size_t DateTime::ToStringUtc(char *buffer, size_t size, const char *format) const
{
    std::tm t;
    gmtime_r(&utc, &t);
    return strftime(buffer, size, format, &t);
}

size_t DateTime::ToStringLocal(char *buffer, size_t size, const char *format) const
{
    std::tm t;
    localtime_r(&utc, &t);
    return strftime(buffer, size, format, &t);
}

// --- Time Components ---

TimeSpan DateTime::GetTimeOfDayUtc() const
{
    std::tm t;
    gmtime_r(&utc, &t);
    return TimeSpan::FromHours(t.tm_hour) +
           TimeSpan::FromMinutes(t.tm_min) +
           TimeSpan::FromSeconds(t.tm_sec);
}

TimeSpan DateTime::GetTimeOfDayLocal() const {
    std::tm t;
    localtime_r(&utc, &t);
    return TimeSpan::FromHours(t.tm_hour) +
           TimeSpan::FromMinutes(t.tm_min) +
           TimeSpan::FromSeconds(t.tm_sec);
}

int DateTime::DayLocal() const
{
    std::tm t;
    localtime_r(&utc, &t);
    return t.tm_mday;
}

int DateTime::MonthLocal() const
{
    std::tm t;
    localtime_r(&utc, &t);
    return t.tm_mon + 1;   // tm_mon = 0–11
}

int DateTime::YearLocal() const
{
    std::tm t;
    localtime_r(&utc, &t);
    return t.tm_year + 1900;
}

int DateTime::HourLocal() const
{
    std::tm t;
    localtime_r(&utc, &t);
    return t.tm_hour;
}

int DateTime::MinuteLocal() const
{
    std::tm t;
    localtime_r(&utc, &t);
    return t.tm_min;
}

int DateTime::SecondLocal() const
{
    std::tm t;
    localtime_r(&utc, &t);
    return t.tm_sec;
}


// --- Raw Access ---

std::time_t DateTime::UtcSeconds() const {
    return utc;
}

// --- Conversion to tm ---

std::tm DateTime::ToUtcTm() const {
    std::tm t;
    gmtime_r(&utc, &t);
    return t;
}

std::tm DateTime::ToLocalTm() const {
    std::tm t;
    localtime_r(&utc, &t);
    return t;
}

// --- Operators ---

DateTime DateTime::operator+(const TimeSpan& ts) const {
    return DateTime(utc + ts.TotalSeconds());
}

DateTime DateTime::operator-(const TimeSpan& ts) const {
    return DateTime(utc - ts.TotalSeconds());
}

DateTime& DateTime::operator+=(const TimeSpan& ts) {
    utc += ts.TotalSeconds();
    return *this;
}

DateTime& DateTime::operator-=(const TimeSpan& ts) {
    utc -= ts.TotalSeconds();
    return *this;
}

TimeSpan DateTime::operator-(const DateTime& other) const {
    return TimeSpan::FromSeconds(utc - other.utc);
}

// --- Comparison Operators ---

bool DateTime::operator==(const DateTime& other) const {
    return utc == other.utc;
}

bool DateTime::operator!=(const DateTime& other) const {
    return utc != other.utc;
}

bool DateTime::operator<(const DateTime& other) const {
    return utc < other.utc;
}

bool DateTime::operator<=(const DateTime& other) const {
    return utc <= other.utc;
}

bool DateTime::operator>(const DateTime& other) const {
    return utc > other.utc;
}

bool DateTime::operator>=(const DateTime& other) const {
    return utc >= other.utc;
}
