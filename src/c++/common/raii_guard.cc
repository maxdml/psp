// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <psp/raii_guard.hh>

#include <utility>

psp::raii_guard::raii_guard(raii_guard &&other) :
    my_dtor(std::move(other.my_dtor))
{
    other.cancel();
}

void psp::raii_guard::cancel() {
    my_dtor = []{};
}
