#pragma once

/// \brief  Constructs an object after filling it's memory with garbage bytes.
///
/// \tparam  T     The type of object to construct
/// \param   args  The values to forward to the object's constructor.
template <typename T, typename... TArgs>
T garbage_fill_and_construct(TArgs... args)
{
    union U
    {
        unsigned char bytes[sizeof(T)];
        T t;
    };

    volatile U u;
    for (int i = 0; i < sizeof(U::bytes); i++)
    {
        u.bytes[i] = 0xCC;
    }

    new (&u.t) T{ std::forward<TArgs>(args)... };

    return u.t;
}

/// \brief  Constructs an object after filling it's memory with garbage bytes.
///
/// \tparam  T     The type of object to construct.
/// \param   t     An initialized object of type T.
/// \param   args  The values to forward to the object's constructor.
template <typename T, typename... TArgs>
void garbage_fill_and_construct(T& t, TArgs... args)
{
    typedef unsigned char bytes[sizeof(T)];

    t.~T();

    bytes* u = reinterpret_cast<bytes*>(&t);
    for (int i = 0; i < sizeof(bytes); i++)
    {
        (*u)[i] = 0xCC;
    }

    new (&t) T{ std::forward<TArgs>(args)... };
}

