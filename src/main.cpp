#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <istream>
#include <memory>
#include <stack>
#include <stdexcept>

#if __cplusplus < 201402L
	// from da interwebz
	template<typename T, typename... Args>
	std::unique_ptr<T> make_unique(Args&&... args) {
		return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
	}
#else
	using std::make_unique;
#endif

class preprocessor {
private:
	struct stream_ptr {
		std::istream *ptr;
		std::unique_ptr<std::ifstream> file;
		stream_ptr(std::istream& p)
				: ptr(&p) {
		}
		stream_ptr(std::unique_ptr<std::ifstream> f)
				: ptr(f.get()), file(std::move(f)) {
		}
	};
	std::stack<stream_ptr> stk;

	bool read(std::string& str) {
		if (stk.empty())
			throw std::runtime_error("can't read\n");
		auto *in = &stk.top();
		while (!(*(in->ptr) >> str)) {
			if (!in->ptr->eof())
				throw std::runtime_error("read error\n");
			stk.pop();
			if (stk.empty())
				return false;
			in = &stk.top();
		}
		return true;
	}
public:
	preprocessor() = delete;
	preprocessor(preprocessor const&) = delete;
	preprocessor(preprocessor&&) = delete;
	preprocessor& operator=(preprocessor const&) = delete;
	preprocessor& operator=(preprocessor&&) = delete;

	preprocessor(std::istream& in) {
		stk.emplace(in);
	}
	preprocessor(std::string const& file) {
		if (file == "-") {
			stk.emplace(std::cin);
			return;
		}
		std::ifstream fin(file);
		if (!fin)
			throw std::runtime_error(file + ": " + std::strerror(errno));
		stk.emplace(fin);
	}

	operator bool() {
		return !stk.empty() && *(stk.top().ptr);
	}
	preprocessor& operator>>(std::string& str) {
		while (read(str)) {
			if (str == "$(") {
				while (read(str))
					if (str == "$)")
						break;
	
			} else if (str == "$[") {
				if (!read(str))
					throw std::runtime_error("no filename\n");
				auto fin = make_unique<std::ifstream>(str);
				if (!*fin)
					throw std::runtime_error(str + ": " + std::strerror(errno));
				if (!read(str) || str != "$]")
					throw std::runtime_error("expected end of file inclusion\n");
				stk.emplace(std::move(fin));
			
			} else break;
		}
		return *this;
	}
};

void eval(preprocessor& pre) {
	std::string str;
	while (pre >> str) {
		// wip
	}
}

int main(int argc, char **argv) {
	if (argc == 1) {
		preprocessor pre(std::cin);
		eval(pre);
	} else for (int i = 1; i < argc; ++i) {
		preprocessor pre(argv[i]);
		eval(pre);
	}
	return EXIT_SUCCESS;
}
