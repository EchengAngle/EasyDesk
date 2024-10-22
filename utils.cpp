#include <iostream>
#include <set>
#include <string>
#include <initializer_list>
#include <memory>
#include <vector>
#include <tuple>
#include <bitset>
#include "printer.h"
#include <typeinfo>
#include <sys/unistd.h>

template<typename T>
class Blob
{
public:
    typedef T value_type;
    typedef typename std::vector<T>::size_type size_type;

    Blob(): data(std::make_shared<std::vector<T>>())
    {};
    Blob(std::initializer_list<T> il);
    // element size
    size_type size() const {return data->size();}
    bool empty() const {return data->empty();}

    // add/remove item
    void push_back(const T& t) {data->push_back(t);}
    // move 
    void push_back(T &&t) {data->push_back(std::move(t));}

    void pop_back(); // remove the last item;

    T& back(); // get the last element
    T& operator[](size_type i);
private:
    std::shared_ptr<std::vector<T>> data;
    void check(size_type i, const std::string& msg) const;
};
template<typename T>
Blob<T>::Blob(std::initializer_list<T> il): data(std::make_shared<std::vector<T>>(il))
{
    // constructor    
}
template<typename T>
void Blob<T>::check(size_type i, const std::string& msg) const
{
    if(i>=data->size())
        throw std::out_of_range(msg);
}
template<typename T>
T& Blob<T>::back()
{
    check(0, "back on empty blob");
    return data->back();
}
template<typename T>
T& Blob<T>::operator[](size_type i)
{
    check(i, "subscript out of range");
    return (*data)[i];
}
template<typename T>
void Blob<T>::pop_back()
{
    check(0, "pop_back from an empty blob");
    data->pop_back();
}

//////////////////////////////////////////////////////////

struct MyComp{
    bool operator()(const std::string &a, const std::string & b) const  
    {
        return a<b ;
    }
};


class Screen{
    public:
    typedef std::string::size_type pos;
    Screen():contents(""),cursor(0), height(0),width(0){}
    Screen(const std::string& str):contents(str),cursor(0), height(0),width(0)
    {}
    char getCursor() const {return contents[cursor];}
    char get(pos p) const
    {
        return contents[p];
    };
    char get(pos ht, pos wd) const{
        std::cout << "ht:" << ht << ",wd:" << wd << std::endl;
        return contents[ht+wd];
    };

    static const std::string Screen::*data()
    {
        return &Screen::contents;
    }
private:
    std::string contents;
    pos cursor;
    pos height, width;
};

int main(int argc, char* argv[])
{     
    std::string tmp;
    ;std::cout << "is empty=" << tmp.empty() << std::endl;
    Screen scrn("This yearis very good for nexton;");
    const std::string Screen::*pData = Screen::data();
    auto s = scrn.*pData;

    std::cout << "s=" << s << std::endl;
    char (Screen::*pm)(Screen::pos, Screen::pos) const;
    pm = &Screen::get;
    auto x = (scrn.*pm)(5,3);
    std::cout << "x=" << x << std::endl;

    

    std::cout << "NOt same a int.\n";

    int i = 10;
    std::cout << "sizeof int=" << sizeof(i) << std::endl;
    return 0;
}
