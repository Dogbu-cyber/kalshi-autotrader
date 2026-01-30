#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace kalshi::logging
{

  /** Supported value types for structured log fields. */
  using LogFieldValue = std::variant<std::string, std::int64_t, std::uint64_t, double, bool,
                                     std::vector<std::string>>;

  /** Single key/value field. */
  struct LogField
  {
    /** Field name. */
    std::string key;
    /** Field value. */
    LogFieldValue value;
  };

  /** Helper for building a list of structured fields. */
  class LogFields
  {
  public:
    /**
     * Add string field.
     * @param key Field name.
     * @param value Field value.
     */
    void add_string(std::string key, std::string value)
    {
      fields_.push_back(LogField{std::move(key), std::move(value)});
    }

    /**
     * Add signed integer field.
     * @param key Field name.
     * @param value Field value.
     */
    void add_int(std::string key, std::int64_t value)
    {
      fields_.push_back(LogField{std::move(key), value});
    }

    /**
     * Add unsigned integer field.
     * @param key Field name.
     * @param value Field value.
     */
    void add_uint(std::string key, std::uint64_t value)
    {
      fields_.push_back(LogField{std::move(key), value});
    }

    /**
     * Add floating point field.
     * @param key Field name.
     * @param value Field value.
     */
    void add_double(std::string key, double value)
    {
      fields_.push_back(LogField{std::move(key), value});
    }

    /**
     * Add boolean field.
     * @param key Field name.
     * @param value Field value.
     */
    void add_bool(std::string key, bool value)
    {
      fields_.push_back(LogField{std::move(key), value});
    }

    /**
     * Add string array field.
     * @param key Field name.
     * @param values Field values.
     */
    void add_string_list(std::string key, std::vector<std::string> values)
    {
      fields_.push_back(LogField{std::move(key), std::move(values)});
    }

    /**
     * Return true if no fields were added.
     * @return True if empty.
     */
    [[nodiscard]] bool empty() const { return fields_.empty(); }
    /**
     * Access raw entries for serialization.
     * @return Vector of LogField entries.
     */
    [[nodiscard]] const std::vector<LogField> &entries() const { return fields_; }

  private:
    std::vector<LogField> fields_;
  };

} // namespace kalshi::logging
