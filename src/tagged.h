#pragma once
#include <compare>

namespace util {

/**
 * Helper template class "Tagged type".
 * It lets you define a strict type based on another type.
 * Example:
 *
 *  struct AddressTag{}; // type tag for a string holding an address
 *  using Address = util::Tagged<std::string, AddressTag>;
 *
 *  struct NameTag{}; // type tag for a string holding a name
 *  using Name = util::Tagged<std::string, NameTag>;
 *
 *  struct Person {
 *      Name name;
 *      Address address;
 *  };
 *
 *  Name name{"Harry Potter"s};
 *  Address address{"4 Privet Drive, Little Whinging, Surrey, England"s};
 *
 * Person p1{name, address}; // OK
 * Person p2{address, name}; // Error: Address and Name are different types
 */
template <typename Value, typename Tag>
class Tagged {
public:
    using ValueType = Value;
    using TagType = Tag;

    explicit Tagged(Value&& v)
        : value_(std::move(v)) {
    }
    explicit Tagged(const Value& v)
        : value_(v) {
    }

    const Value& operator*() const {
        return value_;
    }

    Value& operator*() {
        return value_;
    }

    // In C++20 you can declare a comparison operator for Tagged types this way.
    // It simply calls the corresponding operator for the value_ field.
    auto operator<=>(const Tagged<Value, Tag>&) const = default;

private:
    Value value_;
};

// Hasher for the Tagged type, so that Tagged objects can be stored in unordered containers
template <typename TaggedValue>
struct TaggedHasher {
    size_t operator()(const TaggedValue& value) const {
        // Returns the hash of the value stored inside value
        return std::hash<typename TaggedValue::ValueType>{}(*value);
    }
};

}  // namespace util
