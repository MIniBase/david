#include <iostream>
#include <functional>
#include <memory>
#include <unordered_map>
#include <typeinfo>
#include <boost/any.hpp>

template<typename T>
class Property {
public:
    typedef std::shared_ptr<Property> Ptr;

    Property(const std::string &nane) : name_() {}

    virtual const std::type_info &type() = 0;

    virtual boost::any get(T &) = 0;

    virtual void set(T &, boost::any &) = 0;

    const std::string &name() { return name_; }

private:
    std::string name_;
};


template<typename T, typename MemFn>
class Property_T : public Property<T> {
public:
    Property_T(const std::string &name, MemFn fn)
            : Property<T>(name), fn_(fn) {}

    virtual const std::type_info &type() final {
        return typeid(decltype(fn_(T())));
    };

    boost::any get(T &obj) final {
        return fn_(obj);
    }

    void set(T &obj, boost::any &v) final {
        fn_(obj) = boost::any_cast<decltype(fn_(obj))>(v);
    }


private:
    MemFn fn_;
};

template<typename T, typename MemFn>
Property_T<T, MemFn> *makePropery(const std::string &name, MemFn fn) {
    return new Property_T<T, MemFn>(name, fn);
}


///////////////////////////////////////////////


template<typename T>
struct Struct {
public:
    typedef std::vector<typename Property<T>::Ptr> PropertyContainer;
    typedef std::unordered_map<std::string, typename Property<T>::Ptr> PropertyMap;

    Struct(const std::string &name) : name_(name) {}

    T *clone() { return new T; }

    template<typename PropType>
    Struct<T> &property(const std::string &name, PropType T::* prop) {
        if (!hasPropery(name)) {
            auto ptr = makePropery<T>(name, std::mem_fn(prop));
            properties_[name].reset(ptr);
            properties_ordered_.push_back(ptr);
        }

        return *this;
    }

    bool hasPropery(const std::string &name) {
        return properties_.find(name) != properties_.end();
    }

    size_t propertyCount() { return properties_.size(); }

    PropertyContainer propertyIterator() { return properties_ordered_; }

    typename Property<T>::Ptr getPropertyByName(const std::string& name) {
        auto it = properties_.find(name);
        if (it != properties_.end())
            return it->second;
        return Property<T>::Ptr();
    }

private:
    std::string name_;
    PropertyContainer properties_ordered_;
    PropertyMap properties_;
};


template<typename T>
struct Reflection : public T {
    static Struct<T> *descriptor;

    template<typename T>
    T get(const std::string &fieldname) {
        return descriptor->fields_[fieldname]->get<T>(*this);
    }

    template<typename T>
    void set(const std::string &fieldname, const T &value) {
        descriptor->fields_[fieldname]->set<T>(*this, value);
    }

    void dump() {
        for (auto it = descriptor->fields_.begin(); it != descriptor->fields_.end(); ++it) {
            if (it->second->type() == typeid(int))
                std::cout << it->first << ":" << it->second->get<int>(*this) << std::endl;
            else if (it->second->type() == typeid(std::string))
                std::cout << it->first << ":" << it->second->get<std::string>(*this) << std::endl;
        }
    }
};

template<typename T>
Struct<T> *Reflection<T>::descriptor = NULL;

struct StructFactory {
    static StructFactory &instance() {
        static StructFactory instance_;
        return instance_;
    }

    template<typename T>
    Struct<T> &declare() {
        std::string name = typeid(T).name();
        Struct<T> *desc = new Struct<T>(name);
        structs_[name] = desc;
        return *desc;
    }

    template<typename T>
    Struct<T> *classByType() {
        std::string name = typeid(T).name();
        if (structs_.find(name) != structs_.end())
            return boost::any_cast<Struct<T> *>(structs_[name]);
        return NULL;
    }

private:
    typedef std::unordered_map<std::string, boost::any> Structs;

    Structs structs_;
};


///////////////////////////////////////////////










struct Player {
    int id = 0;
    std::string name;
    int score = 0;
};

// in .cpp

struct Register {
    Register() {
        StructDescriptorFactory::instance().declare<Player>("Player")
                .property("id", &Player::id)
                .property("name", &Player::name);

        Reflection<Player>::descriptor = &StructDescriptorFactory::instance().classByType<Player>("Player");
    }
};

Register reg;


struct PlayerDescriptor {
    PlayerDescriptor() {
        StructDescriptor <Player> descriptor("Player");
        descriptor
                .property("id", &Player::id)
                .property("name", &Player::name);

    }

    typedef std::unordered_map<std::string, PropertyHolderBase < Player> *>
    FieldsMap;

    FieldsMap fields_ = {
            {"id",   createPropertyHolder<Player>(std::mem_fn(&Player::id))},
            {"name", createPropertyHolder<Player>(std::mem_fn(&Player::name))},
    };
};


void test_r() {

    Reflection<Player> p;
    p.name = "david";
    p.set<int>("id", 100);
    p.set<std::string>("name", "wang");

    p.dump();
    std::cout << p.get<int>("id") << std::endl;
    std::cout << p.get<std::string>("name") << std::endl;
}


// 成员变量指针
int get_player_id(Player &p, int Player::* mem) {
    return p.*mem;
}

template<typename Class, typename MemType>
MemType get_field(Class &p, MemType Class::* mem) {
    return p.*mem;
};

void test_n() {
    Player p;
    p.id = 100;
    p.name = "david";

    std::cout << __PRETTY_FUNCTION__ << std::endl;

    std::cout << get_player_id(p, &Player::id) << std::endl;
    std::cout << get_field(p, &Player::id) << std::endl;
    std::cout << get_field(p, &Player::name) << std::endl;

    int v = 10;

    std::cout << typeid(v).name() << std::endl;
    std::cout << typeid(int).name() << std::endl;
}

struct MemberHolderBase {

    template<typename T>
    T get(Player &p) {
        return boost::any_cast<T>(this->getByAny(p));
    }

    template<typename T>
    void set(Player &p, const T &value) {
        boost::any v = value;
        this->setByAny(p, v);
    }


    virtual boost::any getByAny(Player &p) = 0;

    virtual void setByAny(Player &p, boost::any &v) = 0;

};

template<typename MemFun>
struct MemberHolder : public MemberHolderBase {
    MemberHolder(MemFun f) : get(f) {}


    boost::any getByAny(Player &p) {
        return get(p);
    }

    void setByAny(Player &p, boost::any &v) {
        get(p) = boost::any_cast<decltype(get(p))>(v);
    }

    MemFun get;
};

template<typename MemFunc>
MemberHolder<MemFunc> *make_mem(MemFunc get) {
    MemberHolder<MemFunc> *h = new MemberHolder<MemFunc>(get);
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    return h;
}


boost::any getplayer(Player &p, const std::string &mem) {
    if ("id" == mem)
        return std::mem_fn(&Player::id)(p);
    else if ("name" == mem)
        return std::mem_fn(&Player::name)(p);
}


void test_1() {
    std::unordered_multimap<std::string, int> name2id = {
            {"david", 2},
            {"jack",  3},
            {"david", 4},
    };

    std::cout << name2id.count("david") << std::endl;

    auto range = name2id.equal_range("david");
    for (auto it = range.first; it != range.second; ++it) {
        std::cout << it->first << ":" << it->second << std::endl;
    }

    Player p;
    p.id = 1;
    p.name = "david";

    decltype(std::mem_fn(&Player::id)) get_id = std::mem_fn(&Player::id);
    decltype(std::mem_fn(&Player::name)) get_name = std::mem_fn(&Player::name);


    get_id(p) = 100;

    std::cout << get_id(p) << std::endl;
    std::cout << get_name(p) << std::endl;

    std::cout << boost::any_cast<int>(getplayer(p, "id")) << std::endl;
    std::cout << boost::any_cast<std::string>(getplayer(p, "name")) << std::endl;


    std::cout << "------" << std::endl;


    std::unordered_map<std::string, MemberHolderBase *> fields = {
            {"id",   make_mem(std::mem_fn(&Player::id))},
            {"name", make_mem(std::mem_fn(&Player::name))},
    };

    auto getid = make_mem(std::mem_fn(&Player::id));

    std::cout << getid->get(p) << std::endl;
    std::cout << fields["id"]->get<int>(p) << std::endl;
    fields["name"]->set(p, std::string("wang david"));
    std::cout << fields["name"]->get<std::string>(p) << std::endl;
}


int main() {
//    test_n();
//    test_1();
    test_r();
    return 0;
}
