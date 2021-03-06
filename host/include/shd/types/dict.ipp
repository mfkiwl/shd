//
// Copyright 2010-2011 Ettus Research LLC
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef INCLUDED_SHD_TYPES_DICT_IPP
#define INCLUDED_SHD_TYPES_DICT_IPP

#include <shd/exception.hpp>
#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <typeinfo>

namespace shd{

    namespace /*anon*/{
        template<typename Key, typename Val>
        struct key_not_found: shd::key_error{
            key_not_found(const Key &key): shd::key_error(
                str(boost::format(
                    "key \"%s\" not found in dict(%s, %s)"
                    ) % boost::lexical_cast<std::string>(key)
                    % typeid(Key).name() % typeid(Val).name()
                )
            ){
                /* NOP */
            }
        };
    } // namespace /*anon*/

    template <typename Key, typename Val>
    dict<Key, Val>::dict(void){
        /* NOP */
    }

    template <typename Key, typename Val> template <typename InputIterator>
    dict<Key, Val>::dict(InputIterator first, InputIterator last):
        _map(first, last)
    {
        /* NOP */
    }

    template <typename Key, typename Val>
    std::size_t dict<Key, Val>::size(void) const{
        return _map.size();
    }

    template <typename Key, typename Val>
    std::vector<Key> dict<Key, Val>::keys(void) const{
        std::vector<Key> keys;
        BOOST_FOREACH(const pair_t &p, _map){
            keys.push_back(p.first);
        }
        return keys;
    }

    template <typename Key, typename Val>
    std::vector<Val> dict<Key, Val>::vals(void) const{
        std::vector<Val> vals;
        BOOST_FOREACH(const pair_t &p, _map){
            vals.push_back(p.second);
        }
        return vals;
    }

    template <typename Key, typename Val>
    bool dict<Key, Val>::has_key(const Key &key) const{
        BOOST_FOREACH(const pair_t &p, _map){
            if (p.first == key) return true;
        }
        return false;
    }

    template <typename Key, typename Val>
    const Val &dict<Key, Val>::get(const Key &key, const Val &other) const{
        BOOST_FOREACH(const pair_t &p, _map){
            if (p.first == key) return p.second;
        }
        return other;
    }

    template <typename Key, typename Val>
    const Val &dict<Key, Val>::get(const Key &key) const{
        BOOST_FOREACH(const pair_t &p, _map){
            if (p.first == key) return p.second;
        }
        throw key_not_found<Key, Val>(key);
    }

    template <typename Key, typename Val>
    void dict<Key, Val>::set(const Key &key, const Val &val){
        (*this)[key] = val;
    }

    template <typename Key, typename Val>
    const Val &dict<Key, Val>::operator[](const Key &key) const{
        BOOST_FOREACH(const pair_t &p, _map){
            if (p.first == key) return p.second;
        }
        throw key_not_found<Key, Val>(key);
    }

    template <typename Key, typename Val>
    Val &dict<Key, Val>::operator[](const Key &key){
        BOOST_FOREACH(pair_t &p, _map){
            if (p.first == key) return p.second;
        }
        _map.push_back(std::make_pair(key, Val()));
        return _map.back().second;
    }

    template <typename Key, typename Val>
    Val dict<Key, Val>::pop(const Key &key){
        typename std::list<pair_t>::iterator it;
        for (it = _map.begin(); it != _map.end(); it++){
            if (it->first == key){
                Val val = it->second;
                _map.erase(it);
                return val;
            }
        }
        throw key_not_found<Key, Val>(key);
    }

    template <typename Key, typename Val>
    void dict<Key, Val>::update(const dict<Key, Val> &new_dict, bool fail_on_conflict)
    {
        BOOST_FOREACH(const Key &key, new_dict.keys()) {
            if (fail_on_conflict and has_key(key) and get(key) != new_dict[key]) {
                throw shd::value_error(str(
                    boost::format("Option merge conflict: %s:%s != %s:%s")
                    % key % get(key) % key % new_dict[key]
                ));
            }
            set(key, new_dict[key]);
        }
    }

} //namespace shd

#endif /* INCLUDED_SHD_TYPES_DICT_IPP */
