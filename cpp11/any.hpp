#ifndef CPP11_ANY_HPP
#define CPP11_ANY_HPP

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <utility>
#include <type_traits>
#include <typeinfo>


namespace cpp11 {

    // C++11で動作するany型
    class any {
        // 変数を保持するホルダー
        struct holder_base {
            virtual ~holder_base() {}
            virtual const std::type_info& type() const { return typeid(nullptr); }
            virtual std::unique_ptr<holder_base> clone() const { return nullptr; }
        };
        template <class T>
        struct holder : holder_base {
            T value;
            holder(const T& v) : value(v) {}
            holder(T&& v) : value(v) {}
            virtual const std::type_info& type() const { return typeid(T); }
            std::unique_ptr<holder_base> clone() const { return std::unique_ptr<holder_base>(new holder<T>(value)); }
        };
        
        std::unique_ptr<holder_base> data_m;
    public:
        any() : data_m(new holder_base()) {}
        template <class T>
        any(const T& v) : data_m(new holder<typename std::remove_cv<typename std::remove_reference<T>::type>::type>(v)) {}
        template <class T>
        any(T&& v) : data_m(new holder<typename std::remove_cv<typename std::remove_reference<T>::type>::type>(v)) {}
        any(const any& v)  { *this = v; }
        any(any&& v) { *this = std::move(v); }

        // 有効な値を保持しているかの取得
        bool has_value() const noexcept { return this->data_m.get(); }
        
        // 保持している値の型情報を取得
        const std::type_info& type() const noexcept { return this->data_m->type(); }
        
        // 値の代入
        template <class T>
        any& operator=(T&& v) {
            this->data_m = std::unique_ptr<holder_base>(new holder<typename std::remove_cv<typename std::remove_reference<T>::type>::type>(std::forward<T>(v)));
            return *this;
        }
        any& operator=(const any& v) {
            this->data_m = v.data_m->clone();
            return *this;
        }
        any& operator=(any&& v) {
            this->data_m = std::move(v.data_m);
            return *this;
        }

        // anyが保持する値の取得
        template <class T>
        friend T* any_cast(any* v) noexcept {
            if (v == nullptr) return nullptr;
            auto p = dynamic_cast<holder<T>*>(v->data_m.get());
            return p ? std::addressof(p->value) : nullptr;
        }
        template <class T>
        friend const T* any_cast(const any* v) noexcept {
            if (v == nullptr) return nullptr;
            auto p = dynamic_cast<const holder<T>*>(v->data_m.get());
            return p ? std::addressof(p->value) : nullptr;
        }
    };
    
    // anyから値を取り出す
    template <class T>
    inline T any_cast(const any& v) {
        using type = typename std::remove_cv<typename std::remove_reference<T>::type>::type;
        // 面倒なのでコピー構築可能などの判定はしない
        auto p = any_cast<type>(&v);
        if (p) return static_cast<T>(*p);
        throw std::runtime_error("Bad ant cast");
    }
    template <class T>
    inline T any_cast(any& v) {
        using type = typename std::remove_cv<typename std::remove_reference<T>::type>::type;
        // 面倒なのでコピー構築可能などの判定はしない
        auto p = any_cast<type>(&v);
        if (p) return static_cast<T>(*p);
        throw std::runtime_error("Bad ant cast");
    }
}

#endif