/*
 * Copyright (c) 2021, Kyle Pereira <hey@xylepereira.me>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Format.h>
#include <AK/Function.h>
#include <AK/Tuple.h>
#include <AK/Variant.h>
#include <LibCore/DateTime.h>
#include <LibCore/EventLoop.h>
#include <LibCore/Object.h>
#include <utility>

namespace IMAP {
enum class CommandType {
    Capability,
    List,
    Login,
    Logout,
    Noop,
    Select,
};

enum class MailboxFlag : unsigned {
    All = 1u << 0,
    Drafts = 1u << 1,
    Flagged = 1u << 2,
    HasChildren = 1u << 3,
    HasNoChildren = 1u << 4,
    Important = 1u << 5,
    Junk = 1u << 6,
    Marked = 1u << 7,
    NoInferiors = 1u << 8,
    NoSelect = 1u << 9,
    Sent = 1u << 10,
    Trash = 1u << 11,
    Unmarked = 1u << 12,
    Unknown = 1u << 13,
};

enum class ResponseType : unsigned {
    Capability = 1u << 0,
    List = 1u << 1,
    Exists = 1u << 2,
    Recent = 1u << 3,
    Flags = 1u << 4,
    UIDNext = 1u << 5,
    UIDValidity = 1u << 6,
    Unseen = 1u << 7,
    PermanentFlags = 1u << 8,
    Bye = 1u << 13,
};

class Parser;

struct Command {
public:
    CommandType type;
    int tag;
    Vector<String> args;
};

enum class ResponseStatus {
    Bad,
    No,
    OK,
};

struct ListItem {
    unsigned flags;
    String reference;
    String name;
};

class ResponseData {
public:
    [[nodiscard]] unsigned response_type() const
    {
        return m_response_type;
    }

    ResponseData()
        : m_response_type(0)
    {
    }

    ResponseData(ResponseData&) = delete;
    ResponseData(ResponseData&&) = default;
    ResponseData& operator=(const ResponseData&) = delete;
    ResponseData& operator=(ResponseData&&) = default;

    [[nodiscard]] bool contains_response_type(ResponseType response_type) const
    {
        return (static_cast<unsigned>(response_type) & m_response_type) != 0;
    }

    void add_response_type(ResponseType response_type)
    {
        m_response_type = m_response_type | static_cast<unsigned>(response_type);
    }

    void add_capabilities(Vector<String>&& capabilities)
    {
        m_capabilities = move(capabilities);
        add_response_type(ResponseType::Capability);
    }

    Vector<String>& capabilities()
    {
        VERIFY(contains_response_type(ResponseType::Capability));
        return m_capabilities;
    }

    void add_list_item(ListItem&& item)
    {
        add_response_type(ResponseType::List);
        m_list_items.append(move(item));
    }

    Vector<ListItem>& list_items()
    {
        VERIFY(contains_response_type(ResponseType::List));
        return m_list_items;
    }

    void set_exists(unsigned exists)
    {
        add_response_type(ResponseType::Exists);
        m_exists = exists;
    }

    [[nodiscard]] unsigned exists() const
    {
        VERIFY(contains_response_type(ResponseType::Exists));
        return m_exists;
    }

    void set_recent(unsigned recent)
    {
        add_response_type(ResponseType::Recent);
        m_recent = recent;
    }

    [[nodiscard]] unsigned recent() const
    {
        VERIFY(contains_response_type(ResponseType::Recent));
        return m_recent;
    }

    void set_uid_next(unsigned uid_next)
    {
        add_response_type(ResponseType::UIDNext);
        m_uid_next = uid_next;
    }

    [[nodiscard]] unsigned uid_next() const
    {
        VERIFY(contains_response_type(ResponseType::UIDNext));
        return m_uid_next;
    }

    void set_uid_validity(unsigned uid_validity)
    {
        add_response_type(ResponseType::UIDValidity);
        m_uid_validity = uid_validity;
    }

    [[nodiscard]] unsigned uid_validity() const
    {
        VERIFY(contains_response_type(ResponseType::UIDValidity));
        return m_uid_validity;
    }

    void set_unseen(unsigned unseen)
    {
        add_response_type(ResponseType::Unseen);
        m_unseen = unseen;
    }

    [[nodiscard]] unsigned unseen() const
    {
        VERIFY(contains_response_type(ResponseType::Unseen));
        return m_unseen;
    }

    void set_flags(Vector<String>&& flags)
    {
        m_response_type |= static_cast<unsigned>(ResponseType::Flags);
        m_flags = move(flags);
    }

    Vector<String>& flags()
    {
        VERIFY(contains_response_type(ResponseType::Flags));
        return m_flags;
    }

    void set_permanent_flags(Vector<String>&& flags)
    {
        add_response_type(ResponseType::PermanentFlags);
        m_permanent_flags = move(flags);
    }

    Vector<String>& permanent_flags()
    {
        VERIFY(contains_response_type(ResponseType::PermanentFlags));
        return m_permanent_flags;
    }

    void set_bye(Optional<String> message)
    {
        add_response_type(ResponseType::Bye);
        m_bye_message = move(message);
    }

    Optional<String>& bye_message()
    {
        VERIFY(contains_response_type(ResponseType::Bye));
        return m_bye_message;
    }

private:
    unsigned m_response_type;

    Vector<String> m_capabilities;
    Vector<ListItem> m_list_items;

    unsigned m_recent {};
    unsigned m_exists {};

    unsigned m_uid_next {};
    unsigned m_uid_validity {};
    unsigned m_unseen {};
    Vector<String> m_permanent_flags;
    Vector<String> m_flags;
    Optional<String> m_bye_message;
};

class SolidResponse {
    // Parser is allowed to set up fields
    friend class Parser;

public:
    ResponseStatus status() { return m_status; }

    int tag() const { return m_tag; }

    ResponseData& data() { return m_data; }

    String response_text() { return m_response_text; };

    SolidResponse()
        : SolidResponse(ResponseStatus::Bad, -1)
    {
    }

    SolidResponse(ResponseStatus status, int tag)
        : m_status(status)
        , m_tag(tag)
        , m_data(ResponseData())
    {
    }

private:
    ResponseStatus m_status;
    String m_response_text;
    unsigned m_tag;

    ResponseData m_data;
};

struct ContinueRequest {
    String data;
};

template<typename Result>
class Promise : public Core::Object {
    C_OBJECT(Promise);

private:
    Optional<Result> m_pending;

public:
    Function<void(Result&)> on_resolved;

    void resolve(Result&& result)
    {
        m_pending = move(result);
        if (on_resolved)
            on_resolved(m_pending.value());
    }

    bool is_resolved()
    {
        return m_pending.has_value();
    };

    Result await()
    {
        while (!is_resolved()) {
            Core::EventLoop::current().pump();
        }
        return m_pending.release_value();
    }

    // Converts a Promise<A> to a Promise<B> using a function func: A -> B
    template<typename T>
    RefPtr<Promise<T>> map(T func(Result&))
    {
        RefPtr<Promise<T>> new_promise = Promise<T>::construct();
        on_resolved = [new_promise, func](Result& result) mutable {
            auto t = func(result);
            new_promise->resolve(move(t));
        };
        return new_promise;
    }
};
using Response = Variant<SolidResponse, ContinueRequest>;
}

// An RFC 2822 message
// https://datatracker.ietf.org/doc/html/rfc2822
struct Message {
    String data;
};