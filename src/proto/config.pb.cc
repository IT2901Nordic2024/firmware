// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: config.proto
// Protobuf C++ Version: 5.26.1

#include "config.pb.h"

#include <algorithm>
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/extension_set.h"
#include "google/protobuf/wire_format_lite.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/generated_message_reflection.h"
#include "google/protobuf/reflection_ops.h"
#include "google/protobuf/wire_format.h"
#include "google/protobuf/generated_message_tctable_impl.h"
// @@protoc_insertion_point(includes)

// Must be included last.
#include "google/protobuf/port_def.inc"
PROTOBUF_PRAGMA_INIT_SEG
namespace _pb = ::google::protobuf;
namespace _pbi = ::google::protobuf::internal;
namespace _fl = ::google::protobuf::internal::field_layout;

inline constexpr Config::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : timestamp_{0},
        id_{0},
        side_{0},
        type_{static_cast< ::Type >(0)},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR Config::Config(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct ConfigDefaultTypeInternal {
  PROTOBUF_CONSTEXPR ConfigDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~ConfigDefaultTypeInternal() {}
  union {
    Config _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 ConfigDefaultTypeInternal _Config_default_instance_;
static ::_pb::Metadata file_level_metadata_config_2eproto[1];
static const ::_pb::EnumDescriptor* file_level_enum_descriptors_config_2eproto[1];
static constexpr const ::_pb::ServiceDescriptor**
    file_level_service_descriptors_config_2eproto = nullptr;
const ::uint32_t
    TableStruct_config_2eproto::offsets[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
        protodesc_cold) = {
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::Config, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::Config, _impl_.timestamp_),
        PROTOBUF_FIELD_OFFSET(::Config, _impl_.id_),
        PROTOBUF_FIELD_OFFSET(::Config, _impl_.side_),
        PROTOBUF_FIELD_OFFSET(::Config, _impl_.type_),
};

static const ::_pbi::MigrationSchema
    schemas[] ABSL_ATTRIBUTE_SECTION_VARIABLE(protodesc_cold) = {
        {0, -1, -1, sizeof(::Config)},
};
static const ::_pb::Message* const file_default_instances[] = {
    &::_Config_default_instance_._instance,
};
const char descriptor_table_protodef_config_2eproto[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
    protodesc_cold) = {
    "\n\014config.proto\"J\n\006Config\022\021\n\ttimestamp\030\001 "
    "\001(\005\022\n\n\002id\030\002 \001(\005\022\014\n\004side\030\003 \001(\005\022\023\n\004type\030\004 "
    "\001(\0162\005.Type*.\n\004Type\022\021\n\rERROR_INVALID\020\000\022\t\n"
    "\005COUNT\020\001\022\010\n\004TIME\020\002b\006proto3"
};
static ::absl::once_flag descriptor_table_config_2eproto_once;
const ::_pbi::DescriptorTable descriptor_table_config_2eproto = {
    false,
    false,
    146,
    descriptor_table_protodef_config_2eproto,
    "config.proto",
    &descriptor_table_config_2eproto_once,
    nullptr,
    0,
    1,
    schemas,
    file_default_instances,
    TableStruct_config_2eproto::offsets,
    file_level_metadata_config_2eproto,
    file_level_enum_descriptors_config_2eproto,
    file_level_service_descriptors_config_2eproto,
};

// This function exists to be marked as weak.
// It can significantly speed up compilation by breaking up LLVM's SCC
// in the .pb.cc translation units. Large translation units see a
// reduction of more than 35% of walltime for optimized builds. Without
// the weak attribute all the messages in the file, including all the
// vtables and everything they use become part of the same SCC through
// a cycle like:
// GetMetadata -> descriptor table -> default instances ->
//   vtables -> GetMetadata
// By adding a weak function here we break the connection from the
// individual vtables back into the descriptor table.
PROTOBUF_ATTRIBUTE_WEAK const ::_pbi::DescriptorTable* descriptor_table_config_2eproto_getter() {
  return &descriptor_table_config_2eproto;
}
const ::google::protobuf::EnumDescriptor* Type_descriptor() {
  ::google::protobuf::internal::AssignDescriptors(&descriptor_table_config_2eproto);
  return file_level_enum_descriptors_config_2eproto[0];
}
PROTOBUF_CONSTINIT const uint32_t Type_internal_data_[] = {
    196608u, 0u, };
bool Type_IsValid(int value) {
  return 0 <= value && value <= 2;
}
// ===================================================================

class Config::_Internal {
 public:
};

Config::Config(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:Config)
}
Config::Config(
    ::google::protobuf::Arena* arena, const Config& from)
    : Config(arena) {
  MergeFrom(from);
}
inline PROTOBUF_NDEBUG_INLINE Config::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : _cached_size_{0} {}

inline void Config::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  ::memset(reinterpret_cast<char *>(&_impl_) +
               offsetof(Impl_, timestamp_),
           0,
           offsetof(Impl_, type_) -
               offsetof(Impl_, timestamp_) +
               sizeof(Impl_::type_));
}
Config::~Config() {
  // @@protoc_insertion_point(destructor:Config)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void Config::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
Config::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              nullptr,  // OnDemandRegisterArenaDtor
              PROTOBUF_FIELD_OFFSET(Config, _impl_._cached_size_),
              false,
          },
          &Config::MergeImpl,
          &Config::kDescriptorMethods,
      };
  return &_data_;
}
PROTOBUF_NOINLINE void Config::Clear() {
// @@protoc_insertion_point(message_clear_start:Config)
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  ::memset(&_impl_.timestamp_, 0, static_cast<::size_t>(
      reinterpret_cast<char*>(&_impl_.type_) -
      reinterpret_cast<char*>(&_impl_.timestamp_)) + sizeof(_impl_.type_));
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

const char* Config::_InternalParse(
    const char* ptr, ::_pbi::ParseContext* ctx) {
  ptr = ::_pbi::TcParser::ParseLoop(this, ptr, ctx, &_table_.header);
  return ptr;
}


PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<2, 4, 0, 0, 2> Config::_table_ = {
  {
    0,  // no _has_bits_
    0, // no _extensions_
    4, 24,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967280,  // skipmap
    offsetof(decltype(_table_), field_entries),
    4,  // num_field_entries
    0,  // num_aux_entries
    offsetof(decltype(_table_), field_names),  // no aux_entries
    &_Config_default_instance_._instance,
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::Config>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // .Type type = 4;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(Config, _impl_.type_), 63>(),
     {32, 63, 0, PROTOBUF_FIELD_OFFSET(Config, _impl_.type_)}},
    // int32 timestamp = 1;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(Config, _impl_.timestamp_), 63>(),
     {8, 63, 0, PROTOBUF_FIELD_OFFSET(Config, _impl_.timestamp_)}},
    // int32 id = 2;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(Config, _impl_.id_), 63>(),
     {16, 63, 0, PROTOBUF_FIELD_OFFSET(Config, _impl_.id_)}},
    // int32 side = 3;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint32_t, offsetof(Config, _impl_.side_), 63>(),
     {24, 63, 0, PROTOBUF_FIELD_OFFSET(Config, _impl_.side_)}},
  }}, {{
    65535, 65535
  }}, {{
    // int32 timestamp = 1;
    {PROTOBUF_FIELD_OFFSET(Config, _impl_.timestamp_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kInt32)},
    // int32 id = 2;
    {PROTOBUF_FIELD_OFFSET(Config, _impl_.id_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kInt32)},
    // int32 side = 3;
    {PROTOBUF_FIELD_OFFSET(Config, _impl_.side_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kInt32)},
    // .Type type = 4;
    {PROTOBUF_FIELD_OFFSET(Config, _impl_.type_), 0, 0,
    (0 | ::_fl::kFcSingular | ::_fl::kOpenEnum)},
  }},
  // no aux_entries
  {{
  }},
};

::uint8_t* Config::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:Config)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // int32 timestamp = 1;
  if (this->_internal_timestamp() != 0) {
    target = ::google::protobuf::internal::WireFormatLite::
        WriteInt32ToArrayWithField<1>(
            stream, this->_internal_timestamp(), target);
  }

  // int32 id = 2;
  if (this->_internal_id() != 0) {
    target = ::google::protobuf::internal::WireFormatLite::
        WriteInt32ToArrayWithField<2>(
            stream, this->_internal_id(), target);
  }

  // int32 side = 3;
  if (this->_internal_side() != 0) {
    target = ::google::protobuf::internal::WireFormatLite::
        WriteInt32ToArrayWithField<3>(
            stream, this->_internal_side(), target);
  }

  // .Type type = 4;
  if (this->_internal_type() != 0) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteEnumToArray(
        4, this->_internal_type(), target);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:Config)
  return target;
}

::size_t Config::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:Config)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  // int32 timestamp = 1;
  if (this->_internal_timestamp() != 0) {
    total_size += ::_pbi::WireFormatLite::Int32SizePlusOne(
        this->_internal_timestamp());
  }

  // int32 id = 2;
  if (this->_internal_id() != 0) {
    total_size += ::_pbi::WireFormatLite::Int32SizePlusOne(
        this->_internal_id());
  }

  // int32 side = 3;
  if (this->_internal_side() != 0) {
    total_size += ::_pbi::WireFormatLite::Int32SizePlusOne(
        this->_internal_side());
  }

  // .Type type = 4;
  if (this->_internal_type() != 0) {
    total_size += 1 +
                  ::_pbi::WireFormatLite::EnumSize(this->_internal_type());
  }

  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}


void Config::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<Config*>(&to_msg);
  auto& from = static_cast<const Config&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:Config)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  if (from._internal_timestamp() != 0) {
    _this->_impl_.timestamp_ = from._impl_.timestamp_;
  }
  if (from._internal_id() != 0) {
    _this->_impl_.id_ = from._impl_.id_;
  }
  if (from._internal_side() != 0) {
    _this->_impl_.side_ = from._impl_.side_;
  }
  if (from._internal_type() != 0) {
    _this->_impl_.type_ = from._impl_.type_;
  }
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void Config::CopyFrom(const Config& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:Config)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

PROTOBUF_NOINLINE bool Config::IsInitialized() const {
  return true;
}

void Config::InternalSwap(Config* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  ::google::protobuf::internal::memswap<
      PROTOBUF_FIELD_OFFSET(Config, _impl_.type_)
      + sizeof(Config::_impl_.type_)
      - PROTOBUF_FIELD_OFFSET(Config, _impl_.timestamp_)>(
          reinterpret_cast<char*>(&_impl_.timestamp_),
          reinterpret_cast<char*>(&other->_impl_.timestamp_));
}

::google::protobuf::Metadata Config::GetMetadata() const {
  return ::_pbi::AssignDescriptors(&descriptor_table_config_2eproto_getter,
                                   &descriptor_table_config_2eproto_once,
                                   file_level_metadata_config_2eproto[0]);
}
// @@protoc_insertion_point(namespace_scope)
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google
// @@protoc_insertion_point(global_scope)
PROTOBUF_ATTRIBUTE_INIT_PRIORITY2
static ::std::false_type _static_init_ PROTOBUF_UNUSED =
    (::_pbi::AddDescriptors(&descriptor_table_config_2eproto),
     ::std::false_type{});
#include "google/protobuf/port_undef.inc"
