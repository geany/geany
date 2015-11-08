-module(test).

-export([function1/0, function1/2]).

-type type1() :: binary().

-record(record1, {name :: list(), value :: list()}).

-define(DEFINE_1, function1()).
-define(DEFINE_2(A, B), function2(A, B)).

function1() ->
    ok.

function1(A, B) ->
    A + B.
