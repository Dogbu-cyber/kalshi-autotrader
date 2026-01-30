#include "kalshi/md/parse/event_parser.hpp"

#include <chrono>
#include <limits>
#include <optional>
#include <string>
#include <vector>

#include <simdjson.h>

#include "kalshi/md/parse/json_fields.hpp"

namespace kalshi::md {

namespace {

std::expected<std::string, ParseError>
get_string(simdjson::ondemand::object &obj, std::string_view key) {
  auto field = obj[key];
  if (field.error()) {
    return std::unexpected(ParseError::MissingField);
  }
  auto s = field.get_string();
  if (s.error()) {
    return std::unexpected(ParseError::InvalidField);
  }
  return std::string(s.value());
}

std::expected<std::int64_t, ParseError> get_int(simdjson::ondemand::object &obj,
                                                std::string_view key) {
  auto field = obj[key];
  if (field.error()) {
    return std::unexpected(ParseError::MissingField);
  }
  auto val = field.get_int64();
  if (val.error()) {
    return std::unexpected(ParseError::InvalidField);
  }
  return val.value();
}

std::expected<BookSide, ParseError> parse_side(std::string_view side) {
  if (side == VALUE_SIDE_YES) {
    return BookSide::Yes;
  }
  if (side == VALUE_SIDE_NO) {
    return BookSide::No;
  }
  return std::unexpected(ParseError::InvalidField);
}

std::expected<std::vector<PriceLevel>, ParseError>
parse_levels(simdjson::ondemand::object &obj, std::string_view key) {
  auto field = obj[key];
  if (field.error()) {
    return std::unexpected(ParseError::MissingField);
  }
  auto arr = field.get_array();
  if (arr.error()) {
    return std::unexpected(ParseError::InvalidField);
  }

  std::vector<PriceLevel> levels;
  for (auto entry : arr) {
    auto pair = entry.get_array();
    if (pair.error()) {
      return std::unexpected(ParseError::InvalidField);
    }
    auto it = pair.begin();
    if (it == pair.end()) {
      return std::unexpected(ParseError::InvalidField);
    }
    auto price = (*it).get_int64();
    ++it;
    if (it == pair.end()) {
      return std::unexpected(ParseError::InvalidField);
    }
    auto size = (*it).get_int64();
    if (price.error() || size.error()) {
      return std::unexpected(ParseError::InvalidField);
    }
    if (price.value() < 0 || price.value() > PRICE_MAX) {
      return std::unexpected(ParseError::InvalidField);
    }
    if (size.value() < 0 ||
        size.value() >
            static_cast<std::int64_t>(std::numeric_limits<Size>::max())) {
      return std::unexpected(ParseError::InvalidField);
    }
    levels.push_back(PriceLevel{static_cast<Price>(price.value()),
                                static_cast<Size>(size.value())});
  }

  return levels;
}

using DocumentResult = simdjson::simdjson_result<simdjson::ondemand::document>;

std::expected<Sequence, ParseError> get_sequence(DocumentResult &doc) {
  auto field = doc[FIELD_SEQ];
  if (field.error()) {
    return std::unexpected(ParseError::MissingField);
  }
  auto val = field.get_uint64();
  if (val.error()) {
    return std::unexpected(ParseError::InvalidField);
  }
  return val.value();
}

std::expected<simdjson::ondemand::object, ParseError>
get_message_object(DocumentResult &doc) {
  auto msg = doc[FIELD_MSG].get_object();
  if (msg.error()) {
    return std::unexpected(ParseError::MissingField);
  }
  return msg.value();
}

std::optional<std::string> get_optional_string(simdjson::ondemand::object &obj,
                                               std::string_view key) {
  auto field = obj[key];
  if (field.error()) {
    return std::nullopt;
  }
  auto val = field.get_string();
  if (val.error()) {
    return std::nullopt;
  }
  return std::string(val.value());
}

Timestamp parse_optional_timestamp(simdjson::ondemand::object &obj) {
  auto field = obj[FIELD_TIMESTAMP];
  if (field.error()) {
    return Timestamp{0};
  }
  auto val = field.get_int64();
  if (val.error()) {
    return Timestamp{0};
  }
  return std::chrono::duration_cast<Timestamp>(
      std::chrono::seconds(val.value()));
}

struct SnapshotFields {
  MarketTicker market;
  std::vector<PriceLevel> yes;
  std::vector<PriceLevel> no;
};

struct DeltaFields {
  MarketTicker market;
  Price price;
  Delta delta;
  BookSide side;
  std::optional<std::string> client_order_id;
};

struct TradeFields {
  MarketTicker market;
  Price yes_price;
  Price no_price;
  Count count;
  BookSide taker_side;
  Timestamp ts;
};

std::expected<SnapshotFields, ParseError>
parse_snapshot_fields(simdjson::ondemand::object &obj) {
  auto market = get_string(obj, FIELD_MARKET_TICKER);
  if (!market) {
    return std::unexpected(market.error());
  }

  auto yes = parse_levels(obj, FIELD_YES);
  if (!yes) {
    return std::unexpected(yes.error());
  }

  auto no = parse_levels(obj, FIELD_NO);
  if (!no) {
    return std::unexpected(no.error());
  }

  return SnapshotFields{.market = std::move(*market),
                        .yes = std::move(*yes),
                        .no = std::move(*no)};
}

std::expected<DeltaFields, ParseError>
parse_delta_fields(simdjson::ondemand::object &obj) {
  auto market = get_string(obj, FIELD_MARKET_TICKER);
  if (!market) {
    return std::unexpected(market.error());
  }

  auto price = get_int(obj, FIELD_PRICE);
  if (!price) {
    return std::unexpected(price.error());
  }

  auto delta = get_int(obj, FIELD_DELTA);
  if (!delta) {
    return std::unexpected(delta.error());
  }

  auto side_str = get_string(obj, FIELD_SIDE);
  if (!side_str) {
    return std::unexpected(side_str.error());
  }
  auto side = parse_side(*side_str);
  if (!side) {
    return std::unexpected(side.error());
  }

  if (*price < 0 || *price > PRICE_MAX) {
    return std::unexpected(ParseError::InvalidField);
  }
  if (*delta < std::numeric_limits<Delta>::min() ||
      *delta > std::numeric_limits<Delta>::max()) {
    return std::unexpected(ParseError::InvalidField);
  }

  return DeltaFields{.market = std::move(*market),
                     .price = static_cast<Price>(*price),
                     .delta = static_cast<Delta>(*delta),
                     .side = *side,
                     .client_order_id =
                         get_optional_string(obj, FIELD_CLIENT_ORDER_ID)};
}

std::expected<TradeFields, ParseError>
parse_trade_fields(simdjson::ondemand::object &obj) {
  auto market = get_string(obj, FIELD_MARKET_TICKER);
  if (!market) {
    return std::unexpected(market.error());
  }

  auto yes_price = get_int(obj, FIELD_YES_PRICE);
  if (!yes_price) {
    return std::unexpected(yes_price.error());
  }

  auto no_price = get_int(obj, FIELD_NO_PRICE);
  if (!no_price) {
    return std::unexpected(no_price.error());
  }

  auto count = get_int(obj, FIELD_COUNT);
  if (!count) {
    return std::unexpected(count.error());
  }

  auto side_str = get_string(obj, FIELD_TAKER_SIDE);
  if (!side_str) {
    return std::unexpected(side_str.error());
  }
  auto side = parse_side(*side_str);
  if (!side) {
    return std::unexpected(side.error());
  }

  if (*yes_price < 0 || *yes_price > PRICE_MAX || *no_price < 0 ||
      *no_price > PRICE_MAX) {
    return std::unexpected(ParseError::InvalidField);
  }
  if (*count < 0 || *count > std::numeric_limits<Count>::max()) {
    return std::unexpected(ParseError::InvalidField);
  }

  return TradeFields{.market = std::move(*market),
                     .yes_price = static_cast<Price>(*yes_price),
                     .no_price = static_cast<Price>(*no_price),
                     .count = static_cast<Count>(*count),
                     .taker_side = *side,
                     .ts = parse_optional_timestamp(obj)};
}

} // namespace

std::expected<OrderbookSnapshot, ParseError>
parse_orderbook_snapshot(std::string_view json) {
  simdjson::ondemand::parser parser;
  simdjson::padded_string padded(json.data(), json.size());
  auto doc = parser.iterate(padded);
  if (doc.error()) {
    return std::unexpected(ParseError::InvalidJson);
  }

  auto seq = get_sequence(doc);
  if (!seq) {
    return std::unexpected(seq.error());
  }

  auto msg = get_message_object(doc);
  if (!msg) {
    return std::unexpected(msg.error());
  }

  auto fields = parse_snapshot_fields(*msg);
  if (!fields) {
    return std::unexpected(fields.error());
  }

  return OrderbookSnapshot{.market_ticker = std::move(fields->market),
                           .sequence = *seq,
                           .yes = std::move(fields->yes),
                           .no = std::move(fields->no),
                           .ts = Timestamp{0}};
}

std::expected<OrderbookDelta, ParseError>
parse_orderbook_delta(std::string_view json) {
  simdjson::ondemand::parser parser;
  simdjson::padded_string padded(json.data(), json.size());
  auto doc = parser.iterate(padded);
  if (doc.error()) {
    return std::unexpected(ParseError::InvalidJson);
  }

  auto seq = get_sequence(doc);
  if (!seq) {
    return std::unexpected(seq.error());
  }

  auto msg = get_message_object(doc);
  if (!msg) {
    return std::unexpected(msg.error());
  }

  auto fields = parse_delta_fields(*msg);
  if (!fields) {
    return std::unexpected(fields.error());
  }

  return OrderbookDelta{.market_ticker = std::move(fields->market),
                        .sequence = *seq,
                        .price = fields->price,
                        .delta = fields->delta,
                        .side = fields->side,
                        .client_order_id = fields->client_order_id,
                        .ts = Timestamp{0}};
}

std::expected<TradeEvent, ParseError> parse_trade_event(std::string_view json) {
  simdjson::ondemand::parser parser;
  simdjson::padded_string padded(json.data(), json.size());
  auto doc = parser.iterate(padded);
  if (doc.error()) {
    return std::unexpected(ParseError::InvalidJson);
  }

  auto msg = get_message_object(doc);
  if (!msg) {
    return std::unexpected(msg.error());
  }

  auto fields = parse_trade_fields(*msg);
  if (!fields) {
    return std::unexpected(fields.error());
  }

  return TradeEvent{.market_ticker = std::move(fields->market),
                    .yes_price = fields->yes_price,
                    .no_price = fields->no_price,
                    .count = fields->count,
                    .taker_side = fields->taker_side,
                    .ts = fields->ts};
}

} // namespace kalshi::md
