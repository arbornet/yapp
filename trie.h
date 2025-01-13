// A basic `trie` class, keyed by string.
// Supports Picospan/YAPP/Fronttalk-style abbreviations.

#pragma once

#include <algorithm>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <variant>

template<typename T>
class trie {
public:
	using iterator = T *;
 	using const_iterator = const T *;

	constexpr explicit trie() : nkeys{}, nkids{}, terminates{} {}
	constexpr trie(trie<T> &&) noexcept = default;

	constexpr bool insert(std::string_view key, T &&data) {
		std::optional<T> d = std::move(data);
		return this->insert_data(key, std::move(d), this->end());
	}

	const_iterator find(std::string_view key) const {
		return this->find_key(key, this->end());
	}

	constexpr const_iterator end(void) const { return nullptr; }

private:
	constexpr explicit trie(const trie &) = delete;

	constexpr size_t keypos(const char c) const {
		if (this->nkids == 0)
			return 0;
		const auto begin = this->keys.get();
		const auto end = begin + this->nkids;
		auto p = std::lower_bound(begin, end, c);
		return std::distance(begin, p);
	}

	constexpr bool insert_data(std::string_view key,
	    std::optional<T> &&data, const_iterator terminates) {
		if (key.empty()) {
			if (this->data.has_value() || this->terminates != this->end())
				return false;
			this->terminates = terminates;
			if (data.has_value()) {
				this->data = std::move(data);
				this->terminates = &*this->data;
			}
			return true;
		}
		const auto c = key.front();
		key.remove_prefix(1);
		if (c == '_') {
			if (this->data.has_value() || this->terminates != this->end())
				return false;
			this->data = std::move(data);
			return this->insert_data(key, {}, &*this->data);
		}
		const auto i = this->keypos(c);
		if (i == this->nkids || this->keys[i] != c) {
			// We need to add a new kid.  First, insert
			// the key into the new place.
			auto newkeys = new char[this->nkids + 1];
			const auto bkeys = this->keys.get();
			const auto ekeys = this->keys.get() + this->nkids;
			std::copy(bkeys, bkeys + i, newkeys);
			std::copy(bkeys + i, ekeys, newkeys + i + 1);
			newkeys[i] = c;

			// Now, insert the new kid.
			auto newkids = new std::unique_ptr<trie<T>>[this->nkids + 1];
			const auto bkids = this->kids.get();
			const auto ekids = bkids + this->nkids;
			std::move(bkids, bkids + i, newkids);
			std::move(bkids + i , ekids, newkids + i + 1);
			newkids[i] = std::make_unique<trie<T>>();

			this->keys.reset(newkeys);
			this->kids.reset(newkids);
			++this->nkids;
		}
		return this->kids[i]->insert_data(key, std::move(data), terminates);
	}

	const_iterator find_key(std::string_view key, const_iterator last) const {
		if (key.empty()) {
			if (this->data.has_value())
				last = &*this->data;
			return last;
		}
		const auto c = key.front();
		const auto i = this->keypos(c);
		if (i == this->nkids)
			return this->end();
		key.remove_prefix(1);
		if (this->terminates == last)
			last = this->end();
		if (this->data.has_value() && this->terminates == this->end())
			last = &*this->data;
		return this->kids[i]->find_key(key, last);
	}

	std::size_t nkeys;
	std::size_t nkids;
	std::unique_ptr<char[]> keys;
	std::unique_ptr<std::unique_ptr<trie>[]> kids;
	std::optional<T> data;
	const_iterator terminates;
};
