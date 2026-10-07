#pragma once

#include "livekit/server/error.h"

#include <google/protobuf/message.h>
#include <google/protobuf/util/json_util.h>

#include <string>

namespace livekit::server::detail::proto {

template <typename Proto, typename Model> Proto FromModel(const Model& model) {
	Proto message;
	google::protobuf::util::JsonParseOptions options;
	options.ignore_unknown_fields = false;
	const auto status =
	    google::protobuf::util::JsonStringToMessage(std::string(model.Json()), &message, options);
	if (!status.ok()) {
		throw Error(ErrorCode::protocol, "invalid " +
		                                     std::string(Proto::descriptor()->full_name()) +
		                                     " JSON: " + status.ToString());
	}
	return message;
}

template <typename Model, typename Proto> Model ToModel(const Proto& message) {
	std::string json;
	google::protobuf::util::JsonPrintOptions options;
	options.preserve_proto_field_names = false;
	const auto status = google::protobuf::util::MessageToJsonString(message, &json, options);
	if (!status.ok()) {
		throw Error(ErrorCode::protocol, "could not encode " +
		                                     std::string(Proto::descriptor()->full_name()) +
		                                     " as JSON: " + status.ToString());
	}
	return Model::FromJson(std::move(json));
}

} // namespace livekit::server::detail::proto
