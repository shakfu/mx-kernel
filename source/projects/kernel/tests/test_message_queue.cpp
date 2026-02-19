#include "doctest.h"
#include "../message_queue.h"

TEST_CASE("ThreadSafeQueue basic operations") {
    mx::ThreadSafeQueue<int> q;

    SUBCASE("starts empty") {
        CHECK(q.empty());
        CHECK(q.size() == 0);
    }

    SUBCASE("push and pop") {
        q.push(42);
        CHECK(!q.empty());
        CHECK(q.size() == 1);

        auto val = q.try_pop();
        REQUIRE(val.has_value());
        CHECK(val.value() == 42);
        CHECK(q.empty());
    }

    SUBCASE("FIFO ordering") {
        q.push(1);
        q.push(2);
        q.push(3);

        CHECK(q.try_pop().value() == 1);
        CHECK(q.try_pop().value() == 2);
        CHECK(q.try_pop().value() == 3);
        CHECK(!q.try_pop().has_value());
    }

    SUBCASE("try_pop on empty returns nullopt") {
        auto val = q.try_pop();
        CHECK(!val.has_value());
    }

    SUBCASE("clear empties the queue") {
        q.push(1);
        q.push(2);
        q.clear();
        CHECK(q.empty());
        CHECK(q.size() == 0);
    }
}

TEST_CASE("ThreadSafeQueue with OutletMessage") {
    mx::ThreadSafeQueue<mx::OutletMessage> q;

    mx::OutletMessage msg;
    msg.selector = "code";
    msg.atoms.push_back(std::string("execute"));
    msg.atoms.push_back(std::string("print(42)"));
    msg.outlet_index = 0;
    msg.execution_counter = 1;

    q.push(std::move(msg));

    auto result = q.try_pop();
    REQUIRE(result.has_value());
    CHECK(result->selector == "code");
    CHECK(result->atoms.size() == 2);
    CHECK(std::get<std::string>(result->atoms[0]) == "execute");
    CHECK(std::get<std::string>(result->atoms[1]) == "print(42)");
    CHECK(result->outlet_index == 0);
    CHECK(result->execution_counter == 1);
}

TEST_CASE("ResultMessage") {
    SUBCASE("normal result") {
        mx::ResultMessage r;
        r.text = "hello";
        CHECK(!r.is_error());
    }

    SUBCASE("error result") {
        mx::ResultMessage r;
        r.error_name = "RuntimeError";
        r.error_value = "something failed";
        CHECK(r.is_error());
    }
}
