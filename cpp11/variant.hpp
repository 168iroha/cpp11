#ifndef CPP11_VARIANT_HPP
#define CPP11_VARIANT_HPP

#include <cstddef>
#include <type_traits>
#include <utility>

namespace cpp11 {

    template <class First, class... Types>
    class variant;

    namespace detail {
    
        template <class T, class First, class... Types>
        struct variant_max_size_type_impl : variant_max_size_type_impl<typename std::conditional<(sizeof(T) > sizeof(First)), T, First>::type, Types...> {};
        template <class T, class First>
        struct variant_max_size_type_impl<T, First> {
            using type = typename std::conditional<(sizeof(T) > sizeof(First)), T, First>::type;
        };
        // sizeof(T)が最大値となる型Tを得る
        template <class First, class... Types>
        struct variant_max_size_type : variant_max_size_type_impl<First, First, Types...> {};   // variant_max_size_type_implの仕様の関係上Firstを複製する
        
        // variantのストレージ
        template <class... Types>
        struct variant_storage {
            using max_sizeof_type = typename detail::variant_max_size_type<Types...>::type;
            alignas(Types...) unsigned char x[sizeof(max_sizeof_type)];
            
            // 領域を割り当てる
            template <class T>
            void assign(T&& v) {
                using type = typename std::decay<T>::type;
                ::new(static_cast<void*>(this->x)) type(std::forward<T>(v));
            }
            // 型Tとしての値の取得
            template <class T>
            T& get() { return *static_cast<T*>(this->x); }
            // 型Tとしての値の取得
            template <class T>
            const T& get() const { return *static_cast<const T*>(this->x); }
        };

        template <std::size_t N, class T, class Variant, bool = std::is_same<typename Variant::first_type, T>::value>
        struct variant_index_impl;
        template <std::size_t N, class T, class First, class Second, class... Types>
        struct variant_index_impl<N, T, variant<First, Second, Types...>, false> {
            static constexpr std::size_t value = variant_index_impl<N, T, variant<Second, Types...>>::value;
        };
        template <std::size_t N, class T, class First, class... Types>
        struct variant_index_impl<N, T, variant<First, Types...>, true> {
            static constexpr std::size_t value = N - sizeof...(Types);
        };
        template <std::size_t N, class T, class First>
        struct variant_index_impl<N, T, variant<First>, false> {
            static_assert("There is no index corresponding to type T.");
        };
        template <class T, class Variant>
        struct variant_index;
        // Types...内でのTのインデックスを得る
        template <class T, class First, class... Types>
        struct variant_index<T, variant<First, Types...>> : variant_index_impl<sizeof...(Types), T, variant<First, Types...>> {};

        // variant型におけるvisitorの取得
        template <class F, class R, class T1>
        inline auto variant_call_visitor() -> R(*)(F&&, typename std::conditional<std::is_const<T1>::value, const void*, void*>::type) {
            return [](F&& f, typename std::conditional<std::is_const<T1>::value, const void*, void*>::type p){ return std::forward<F>(f)(*static_cast<T1*>(p)); };
        }
    }

    template <class First, class... Types>
    class variant {
        // variantのストレージ
        detail::variant_storage<First, Types...> storage_m;
        // 型のインデックス
        std::size_t index_m = 0;
        
        // リソースを破棄するためのvisitのようなもの
        void destroy() {
            static void(*fs[])(void*) = { [](void* p) { static_cast<First*>(p)->~First(); }, [](void* p) { static_cast<Types*>(p)->~Types(); }... };
            fs[this->index()](this->storage_m.x);
        }
        // コピーおよびムーブのためのvisitor
        struct copy_visitor {
            variant& v;     // variant型のキャプチャ
            copy_visitor(variant& v_) : v(v_) {}
            template <class T>
            void operator()(const T& x) { this->v = x; }
        };
        struct move_visitor {
            variant& v;     // variant型のキャプチャ
            move_visitor(variant& v_) : v(v_) {}
            template <class T>
            void operator()(T&& x) { this->v = std::move(x); }
        };
    public:
        variant() { *this = First(); }
        template <class T, typename std::enable_if<!std::is_same<typename std::decay<T>::type, variant>::value, std::nullptr_t>::type = nullptr>
        variant(T&& v) { *this = std::forward<T>(v); }
        variant(const variant& v) { *this = v; }
        variant(variant&& v) { *this = std::move(v); }
        ~variant() { this->destroy(); }
        
        using first_type = First;
        
        // Types...のインデックスの取得
        std::size_t index() const noexcept { return this->index_m; }
        
        // 要素の取得
        void* get() noexcept { return this->storage_m.x; }
        const void* get() const noexcept { return this->storage_m.x; }
        
        // 値の代入
        variant& operator=(const variant& v) {
            visit(copy_visitor(*this), v);
            return *this;
        }
        variant& operator=(variant&& v) {
            visit(move_visitor(*this), v);
            return *this;
        }
        template <class T, typename std::enable_if<!std::is_same<typename std::decay<T>::type, variant>::value, std::nullptr_t>::type = nullptr>
        variant& operator=(T&& v) {
            // 領域を解放
            this->destroy();
            // 領域を割り当てる
            this->storage_m.assign(std::forward<T>(v));
            this->index_m = detail::variant_index<typename std::decay<T>::type, variant>::value;
            return *this;
        }
    };
    
    // variantが現在型Tを保持しているかの判定
    template <class T, class First, class... Types>
    inline bool holds_alternative(const variant<First, Types...>& x) noexcept {
        return detail::variant_index<T, variant<First, Types...>>::value == x.index();
    }

    // variantの保持している要素を取得する
    template <class T, class First, class... Types>
    inline T& get(variant<First, Types...>& v) {
        if (v.index() != detail::variant_index<T, variant<First, Types...>>::value) throw std::runtime_error("Bad variant access");
        return *static_cast<T*>(v.get());
    }
    template <class T, class First, class... Types>
    inline const T& get(const variant<First, Types...>& v) {
        if (v.index() != detail::variant_index<T, variant<First, Types...>>::value) throw std::runtime_error("Bad variant access");
        return *static_cast<T*>(v.get());
    }

    /* 
     * 共通のインターフェースの呼び出し
     * 全てのインターフェースの戻り値型は共通でなければならない
     */
    template <class Visitor, class First, class... Types>
    inline auto visit(Visitor&& vis, variant<First, Types...>& v) -> decltype(vis(std::declval<First&>())) {
        using result_type = decltype(vis(std::declval<First&>()));
        // 関数呼び出しのテーブル
        static result_type(*fs[])(Visitor&&, void*) = { detail::variant_call_visitor<Visitor, result_type, First>(), detail::variant_call_visitor<Visitor, result_type, Types>()... };
        return fs[v.index()](std::forward<Visitor>(vis), v.get());
    }
    template <class Visitor, class First, class... Types>
    inline auto visit(Visitor&& vis, const variant<First, Types...>& v) -> decltype(vis(std::declval<const First&>())) {
        using result_type = decltype(vis(std::declval<const First&>()));
        // 関数呼び出しのテーブル
        static result_type(*fs[])(Visitor&&, const void*) = { detail::variant_call_visitor<Visitor, result_type, const First>(), detail::variant_call_visitor<Visitor, result_type, const Types>()... };
        return fs[v.index()](std::forward<Visitor>(vis), v.get());
    }
}

#endif