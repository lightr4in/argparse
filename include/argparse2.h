#pragma once
//
// @author : Morris Franken
//
// Permission is hereby granted, free of charge, to any person or organization
// obtaining a copy of the software and accompanying documentation covered by
// this license (the "Software") to use, reproduce, display, distribute,
// execute, and transmit the Software, and to prepare derivative works of the
// Software, and to permit third-parties to whom the Software is furnished to
// do so, all subject to the following:
//
// The copyright notices in the Software and this entire statement, including
// the above license grant, this restriction and the following disclaimer,
// must be included in all copies of the Software, in whole or in part, and
// all derivative works of the Software, unless such copies or derivative
// works are solely in the form of machine-executable object code generated by
// a source language processor.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
// SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
// FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
// ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.
#include <vector>
#include <string>
#include <map>
#include <sstream>
#include <ostream>
#include <iterator>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <memory>
#include <optional>
#include <numeric>

#define ARGPARSE_VERSION 2

namespace argparse2 {
    using std::cout, std::cerr, std::endl, std::setw;

#define CONSTRUCTOR(T) T(int argc, char* argv[]) : argparse2::Args(argc, argv) {validate();}

    template<typename T> struct is_vector : public std::false_type {};
    template<typename T, typename A> struct is_vector<std::vector<T, A>> : public std::true_type {};

    template<typename T> struct is_optional : public std::false_type {};
    template<typename T> struct is_optional<std::optional<T>> : public std::true_type {};

    template<typename T> struct is_shared_ptr : public std::false_type {};
    template<typename T> struct is_shared_ptr<std::shared_ptr<T>> : public std::true_type {};

    template<typename... Ignored> std::string toString(const bool &v) {
        return v? "true" : "false"; // special case for boolean to write true/false instead of 1/0
    }
    template<typename T, typename = decltype(std::declval<std::ostream&>() << std::declval<T const&>())> std::string toString(const T &v) {
        return static_cast<std::ostringstream &&>((std::ostringstream() << v)).str();       // https://github.com/stan-dev/math/issues/590#issuecomment-550122627
    }
    template<typename T, typename... Ignored > std::string toString(const T &v, const Ignored &...) {
        return "unknown";
    }

    std::vector<std::string> inline split(const std::string &str) {
        std::vector<std::string> splits;
        for (size_t start=0, end=0; end != std::string::npos; start=end+1) {
            end = str.find(',', start);
            splits.emplace_back(str.substr(start, end - start));
        }
        return splits;
    }

    template<typename T> inline T get(const std::string &v);
    template<> inline std::string get(const std::string &v) { return v; }
    template<> inline char get(const std::string &v) { return v.empty()? throw std::invalid_argument("empty string") : v.size() > 1?  v.substr(0,2) == "0x"? (char)std::stoul(v, nullptr, 16) : (char)std::stoi(v) : v[0]; }
    template<> inline int get(const std::string &v) { return std::stoi(v); }
    template<> inline long get(const std::string &v) { return std::stol(v); }
    template<> inline bool get(const std::string &v) { return v == "true" || v == "TRUE" || v == "1"; }
    template<> inline float get(const std::string &v) { return std::stof(v); }
    template<> inline double get(const std::string &v) { return std::stod(v); }
    template<> inline unsigned char get(const std::string &v) { return get<char>(v); }
    template<> inline unsigned int get(const std::string &v) { return std::stoul(v); }
    template<> inline unsigned long get(const std::string &v) { return std::stoul(v); }
    namespace { // get for container types (e.g. vector, raw pointer, shared_ptr, optional or custom
        using _=std::false_type;
        using U=std::true_type;
        template<typename T> T get_vector_pointer_optional_default(U,_,_,_, const std::string &v) { // vectors
            const std::vector<std::string> splitted = split(v);
            T res(splitted.size());
            if (!v.empty())
                std::transform (splitted.begin(), splitted.end(), res.begin(), get<typename T::value_type>);
            return res;
        }
        template<typename T> T get_vector_pointer_optional_default(_, U, _, _, const std::string &v) { // raw pointers
            return new typename std::remove_pointer<T>::type(get<typename std::remove_pointer<T>::type>(v));
        }
        template<typename T> T get_vector_pointer_optional_default(_, _, U, _, const std::string &v) { // shared pointers
            return std::make_shared<typename T::element_type>(get<typename T::element_type>(v));
        }
        template<typename T> T get_vector_pointer_optional_default(_, _, _, U, const std::string &v) { // std::optionals
            return get<typename T::value_type>(v);
        }
        template<typename T> T get_vector_pointer_optional_default(_, _, _, _, const std::string &v) { // default (use string constructor)
            return T(v);
        }
    }
    template<typename T> inline T get(const std::string &v) { // "if constexpr" are only supported from c++17, so use this to distuingish vectors.
        return get_vector_pointer_optional_default<T>(is_vector<T>{}, std::is_pointer<T>{}, is_shared_ptr<T>{}, is_optional<T>{}, v);
    }

    struct ConvertBase {
        virtual ~ConvertBase() = default;
        virtual void convert(const std::string &v) = 0;
        virtual void set_default(const std::unique_ptr<ConvertBase> &default_value) = 0;
    };

    template <typename T> struct ConvertType : public ConvertBase {
        T data;
        ~ConvertType() override = default;
        ConvertType() : ConvertBase() {};
        explicit ConvertType(const T &value) : ConvertBase(), data(value) {};
        void convert(const std::string &v) override {
            data = get<T>(v);
        }
        void set_default(const std::unique_ptr<ConvertBase> &default_value) override {
//            data = std::static_pointer_cast<ConvertType<T>>(default_value)->data;  // ensure typeid matches...
            data = ((ConvertType<T>*)(default_value.get()))->data;  // ensure typeid matches...
        }
    };

    struct Entry {
        enum ARG_TYPE {ARG, KWARG, FLAG} type;
        std::vector<std::string> keys;
        std::string help;
        std::optional<std::string> value;
        std::optional<std::string> implicit_value;
        std::optional<std::string> default_str;
        std::string error;
        std::unique_ptr<ConvertBase> datap;
        std::unique_ptr<ConvertBase> data_default;
        bool _is_multi_argument = false;

        Entry(ARG_TYPE type, const std::string& key, std::string help, std::optional<std::string> implicit_value=std::nullopt) :
                type(type),
                keys(split(key)),
                help(std::move(help)),
                implicit_value(std::move(implicit_value)) {
        }

        [[nodiscard]] std::string _get_keys() const {
            std::stringstream ss;
            for (size_t i = 0; i < keys.size(); i++)
                ss << (i? "," : "") << (type == ARG? "" : (keys[i].size() > 1? "--" : "-")) + keys[i];
            return ss.str();
        }

        // Allow both string inputs and direct-type inputs. Where a string-input will be converted like it would when using the commandline, and the direct approach is to simply use the value provided
        template <typename T> inline Entry &set_default(const T &default_value) {
            default_str = toString(default_value);
            if constexpr (!(std::is_array<T>::value || std::is_same<typename std::remove_all_extents<T>::type, char>::value)) {
                data_default = std::make_unique<ConvertType<T>>(default_value);
            }
            return *this;
        }

        inline Entry &multi_argument() {
            _is_multi_argument = true;
            return *this;
        }

        // Magically convert the value string to the requested type
        template <typename T> inline operator T&() {
            if constexpr (is_optional<T>::value || std::is_pointer<T>::value || is_shared_ptr<T>::value) {
                // Automatically set the default to nullptr for pointer types and empty for optional types
                default_str = "none";
                if constexpr(is_optional<T>::value) {
                    data_default = std::make_unique<ConvertType<T>>(T{std::nullopt});
                } else {
                    data_default = std::make_unique<ConvertType<T>>((T) nullptr);
                }
            }
            datap = std::make_unique<ConvertType<T>>();
            return ((ConvertType<T>*)(datap.get()))->data;
        };

        void _convert(const std::string &value) {
            try {
                this->value = value;
                datap->convert(value);
            } catch (const std::invalid_argument &e) {
                error = "Invalid argument, could not convert \"" + value + "\" for " + _get_keys() + " (" + help + ")";
            }
        }

        void _apply_default() {
            if (data_default != nullptr) {
                value = *default_str; // for printing
                datap->set_default(data_default);
            } else if (default_str.has_value()) {   // in cases where a string is provided to the `set_default` function
                _convert(default_str.value());
            } else {
                error = "Argument missing: " + _get_keys() + " (" + help + ")";
            }
        }
    };

    class Args {
    private:
        size_t _arg_idx = 0;
        std::string program_name;
        std::vector<std::string> params;
        std::vector<std::shared_ptr<Entry>> all_entries;
        std::map<std::string, std::shared_ptr<Entry>> kwarg_entries;
        std::vector<std::shared_ptr<Entry>> arg_entries;

    public:
        virtual ~Args() = default;

        Args(int argc, char *argv[]) : program_name(argv[0]) {
            params = std::vector<std::string>(argv + 1, argv + argc);
        }

        /* Add a positional argument, the order in which it is defined equals the order in which they are being read.
         * help : Description of the variable
         *
         * Returns a reference to the Entry, which will collapse into the requested type in `Entry::operator T()`
         */
        Entry &arg(const std::string &help) {
            std::shared_ptr<Entry> entry = std::make_shared<Entry>(Entry::ARG, "arg_" + std::to_string(_arg_idx++), help);
            arg_entries.emplace_back(entry);
            all_entries.emplace_back(entry);
            return *entry;
        }

        /* Add a variable argument that takes a variable.
         * key : A comma-separated string, e.g. "k,key", which denotes the short (-k) and long(--key) keys
         * help : Description of the variable
         * implicit_value : Implicit values are used when no value is provided.
         *
         * Returns a reference to the Entry, which will collapse into the requested type in `Entry::operator T()`
         */
        Entry &kwarg(const std::string &key, const std::string &help, const std::optional<std::string>& implicit_value=std::nullopt) {
            std::shared_ptr<Entry> entry = std::make_shared<Entry>(Entry::KWARG, key, help, implicit_value);
            all_entries.emplace_back(entry);
            for (const std::string &k : entry->keys) {
                kwarg_entries[k] = entry;
            }
            return *entry;
        }

        /* Add a flag which will be false by default.
         * key : A comma-separated string, e.g. "k,key", which denotes the short (-k) and long(--key) keys
         * help : Description of the variable
         *
         * As with kwarg, a flag returns a reference to the Entry, which will collapse into a bool
         */
        Entry &flag(const std::string &key, const std::string &help) {
            return kwarg(key, help, "true").set_default<bool>(false);
        }

        virtual void help() const {
            cout << "Usage: " << program_name << " ";
            for (const auto &entry : arg_entries)
                cout << entry->keys[0] << ' ';
            cout << " [options...]" << endl;
            for (const auto &entry : arg_entries) {
                const std::string default_value = entry->default_str.has_value()? " [default: " + *entry->default_str + "]" : "";
                cout << setw(17) << entry->keys[0] << " : " << entry->help << default_value << endl;
            }

            cout << endl << "Options:" << endl;
            for (const auto &entry : all_entries) {
                if (entry->type != Entry::ARG) {
                    const std::string default_value = entry->type == Entry::KWARG ? entry->default_str.has_value() ? "default: " + *entry->default_str : "required" : "";
                    const std::string implicit_value = entry->type == Entry::KWARG && entry->implicit_value.has_value() ? "implicit: " + *entry->implicit_value : "";
                    const std::string info = entry->type == Entry::KWARG ? " [" + implicit_value + (implicit_value.empty() || default_value.empty() ? "" : ", ") + default_value + "]" : "";
                    cout << setw(17) << entry->_get_keys() << " : " << entry->help << info << endl;
                }
            }
        }

        void parse() {
            bool& _help = flag("help", "print help");

            auto is_value = [&](const size_t &i) -> bool {
                return params.size() > i && (params[i][0] != '-' || (params[i].size() > 1 && std::isdigit(params[i][1])));  // check for number to not accidentally mark negative numbers as non-parameter
            };
            auto add_param = [&](size_t &i, const size_t &start) {
                size_t eq_idx = params[i].find('=');  // check if value was passed using the '=' sign
                if (eq_idx != std::string::npos) { // key/value from = notation
                    std::string key = params[i].substr(start, eq_idx - start);
                    std::string value = params[i].substr(eq_idx + 1);
                    auto itt = kwarg_entries.find(key);
                    if (itt != kwarg_entries.end()) {
                        auto &entry = itt->second;
                        entry->_convert(value);
                    } else {
                        cerr << "unrecognised commandline argument: " << key << endl;
                    }
                } else {
                    std::string key = std::string(params[i].substr(start));
                    auto itt = kwarg_entries.find(key);
                    if (itt != kwarg_entries.end()) {
                        auto &entry = itt->second;
                        if (entry->implicit_value.has_value()) {
                            entry->_convert(*entry->implicit_value);
                        } else {
                            if (is_value(i + 1)) {
                                std::string value = params[++i];
                                if (entry->_is_multi_argument) {
                                    while (is_value(i + 1))
                                        value += "," + params[++i];
                                }
                                entry->_convert(value);
                            } else {
                                entry->error = "No value provided for: " + key;
                            }
                        }
                    } else {
                        cerr << "unrecognised commandline argument: " << key << endl;
                    }
                }
            };

            std::vector<std::string> arguments_flat;
            for (size_t i = 0; i < params.size(); i++) {
                if (!is_value(i)) {
                    if (params[i].size() > 1 && params[i][1] == '-') {  // long --
                        add_param(i, 2);
                    } else { // short -
                        const size_t j_end = std::min(params[i].size(), params[i].find('=')) - 1;
                        for (size_t j = 1; j < j_end; j++) { // add possible other flags
                            std::string key = std::string(1, params[i][j]);
                            auto itt = kwarg_entries.find(key);
                            if (itt != kwarg_entries.end()) {
                                auto &entry = itt->second;
                                if (entry->implicit_value.has_value()) {
                                    entry->_convert(*entry->implicit_value);
                                } else {
                                    entry->error = "No (Implicit) value provided for: " + key;
                                }
                            } else {
                                cerr << "unrecognised commandline argument: " << key << endl;
                            }
                        }
                        add_param(i, j_end);
                    }
                } else {
                    arguments_flat.emplace_back(params[i]);
                }
            }

            // Parse all the positional arguments, making sure multi_argument positional arguments are processed last to enable arguments afterwards
            size_t arg_i = 0;
            for (; arg_i < arg_entries.size() && !arg_entries[arg_i]->_is_multi_argument; arg_i++) { // iterate over positional arguments untill a multi-argument is found
                if (arg_i < arguments_flat.size())
                    arg_entries[arg_i]->_convert(arguments_flat[arg_i]);
            }
            size_t arg_j = 1;
            for (size_t j_end = arg_entries.size() - arg_i; arg_j <= j_end; arg_j++) {
                size_t flat_idx = arguments_flat.size() - arg_j;
                if (flat_idx < arguments_flat.size() && flat_idx >= arg_i) {
                    if (arg_entries[arg_entries.size() - arg_j]->_is_multi_argument) {
                        std::stringstream s;
                        copy(&arguments_flat[arg_i],&arguments_flat[flat_idx + 1], std::ostream_iterator<std::string>(s,","));
                        std::string value = s.str();
                        value.back() = '\0'; // remove trailing ','
                        arg_entries[arg_i]->_convert(value);
                    } else {
                        arg_entries[arg_entries.size() - arg_j]->_convert(arguments_flat[flat_idx]);
                    }
                }
            }

            // try to apply default values for arguments which have not been set
            for (const auto &entry : all_entries) {
                if (!entry->value.has_value()) {
                    entry->_apply_default();
                }
            }

            if (_help) {
                help();
                exit(0);
            }
        }

        /* Validate all parameters and also check for the help_flag which was set in this constructor
         * Upon error, it will print the error and exit immediately.
         */
        void validate() {
            parse();

            for (const auto &entry : all_entries) {
                if (!entry->error.empty()) {
                    cerr << entry->error << endl;
                    exit(-1);
                }
            }
        }

        void print() const {
            for (const auto &entry : all_entries) {
                std::string snip = entry->type == Entry::ARG ? "(" + (entry->help.size() > 10 ? entry->help.substr(0, 7) + "..." : entry->help) + ")" : "";
                cout << setw(21) << entry->_get_keys() + snip << " : " << entry->value.value_or("null") << endl;
            }
        }
    };
}