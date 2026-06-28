#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <istream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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
		std::string filename;
		stream_ptr() = delete;
		stream_ptr(std::istream& p)
				: ptr(&p) {
		}
		stream_ptr(std::unique_ptr<std::ifstream>&& f, std::string fname)
				: ptr(f.get()), file(std::move(f)), filename(std::move(fname)) {
		}
	};
	std::vector<stream_ptr> stk;

	bool read(std::string& str) {
		if (stk.empty())
			throw std::runtime_error("can't read\n");
		auto *in = &stk.back();
		while (!(*(in->ptr) >> str)) {
			if (!in->ptr->eof())
				throw std::runtime_error("read error\n");
			stk.pop_back();
			if (stk.empty())
				return false;
			in = &stk.back();
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
		stk.emplace_back(in);
	}
	preprocessor(std::string const& file) {
		if (file == "-") {
			stk.emplace_back(std::cin);
			return;
		}
		auto fin = make_unique<std::ifstream>(file);
		if (!fin)
			throw std::runtime_error(file + ": " + std::strerror(errno));
		stk.emplace_back(std::move(fin), file);
	}

	operator bool() {
		return !stk.empty() && *(stk.back().ptr);
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
				bool found = false;
				for (auto const& it : stk) {
					if (it.filename == str) {
						found = true;
						break;
					}
				}
				if (!found) {
					auto fin = make_unique<std::ifstream>(str);
					if (!*fin)
						throw std::runtime_error(str + ": " + std::strerror(errno));
					std::string brk;
					if (!read(brk) || brk != "$]")
						throw std::runtime_error("expected end of file inclusion\n");
					stk.emplace_back(std::move(fin), str);
				} else {
					// Silent ignore if it appeared once
					if (!read(str) || str != "$]")
						throw std::runtime_error("expected end of file inclusion\n");
				}
			} else break;
		}
		return *this;
	}
};

void eval(preprocessor& pre) {
	struct block {
		std::unordered_set<std::string> vars;
		std::unordered_map<std::string, std::unordered_set<std::string>> disjs;
	};
	std::vector<block> stk(1);
	std::unordered_set<std::string> consts;
	std::string str;

	while (pre >> str) {
		// Open/close a block
		if (str == "${") {
			stk.emplace_back();
		} else if (str == "$}") {
			if (stk.size() == 1)
				throw std::runtime_error("no block to close\n");
			stk.pop_back();
		
		// Variable
		} else if (str == "$v") {
			while (pre >> str) {
				if (str == "$.")
					break;
				for (auto const& it : stk) {
					if (it.vars.find(str) != it.vars.end())
						throw std::runtime_error("variable " + str + " already exists\n");
				}
				stk.back().vars.insert(std::move(str));
			}
		
		// Constant
		} else if (str == "$c") {
			while (pre >> str) {
				if (str == "$.")
					break;
				if (stk.size() != 1)
					throw std::runtime_error("constant declared inside block\n");
				consts.insert(std::move(str));
			}
	
		// Disjoint var condition
		} else if (str == "$d") {
			std::unordered_set<std::string> arr;
			while (pre >> str) {
				if (str == "$.")
					break;
				arr.insert(std::move(str));
			}
			for (auto it = arr.begin(); it != arr.end(); ++it) {
				for (auto jt = std::next(it); jt != arr.end(); ++jt) {
					stk.back().disjs[*it].insert(*jt);
					stk.back().disjs[*jt].insert(*it);
				}
			}
		}
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
