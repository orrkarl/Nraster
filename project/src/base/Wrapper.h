#pragma once

#include "../predefs.h"

namespace nr::base {

template <class WrappedTraits> class UniqueWrapper {
public:
    using Type = typename WrappedTraits::Type;

    explicit UniqueWrapper(Type object);

    UniqueWrapper(const UniqueWrapper&) = delete;
    UniqueWrapper& operator=(const UniqueWrapper&) = delete;

    UniqueWrapper(UniqueWrapper&& other) noexcept;
    UniqueWrapper& operator=(UniqueWrapper&& other) noexcept;

    ~UniqueWrapper();

    operator Type() const;

    [[nodiscard]] bool isValid() const;

private:
    Type m_object;
};

}

#include "Wrapper.inl"