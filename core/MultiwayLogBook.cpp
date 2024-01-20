#include "MultiwayLogBook.h"

namespace YAM
{
    MultiwayLogBook::MultiwayLogBook() {}

    MultiwayLogBook::MultiwayLogBook(std::initializer_list<std::shared_ptr<ILogBook>> books)
        : _books(books)
    {}

    void MultiwayLogBook::add(std::shared_ptr<ILogBook> const& book) {
        _books.push_back(book);
    }

    void MultiwayLogBook::add(LogRecord const& record) {
        if (!mustLogAspect(record.aspect)) return;
        for (auto const& b : _books) {
            b->add(record);
        }
    }

    bool MultiwayLogBook::error() const {
        for (auto const& b : _books) {
            if (b->error()) return true;
        }
        return false;
    }

    void MultiwayLogBook::resetError() {
        for (auto const& b : _books) {
            b->resetError();
        }
    } 
}
