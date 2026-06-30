#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <istream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
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
		if (!*fin)
			throw std::runtime_error(file + ": " + std::strerror(errno));
		stk.emplace_back(std::move(fin), file);
	}

	operator bool() {
		return !stk.empty() && *(stk.back().ptr);
	}
	preprocessor& operator>>(std::string& str) {
		while (read(str)) {
			if (str == "$(") {
				int cmpnd = 1;
				while (read(str)) {
					if (str == "$(") ++cmpnd;
					if (str == "$)") --cmpnd;
					if (!cmpnd)
						break;
				}
	
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
		// Variables and constants
		std::unordered_map<std::string, char> varorconst;
		// Disjoint variable restrictions
		std::unordered_map<std::string, std::unordered_set<std::string>> disjs;
		// Floating hypotheses
		std::unordered_map<std::string, std::string> var2float;
		std::unordered_map<std::string, std::unordered_set<std::string>> float2vars;
		// Essential hypothesis
		std::unordered_map<std::string, std::pair<std::string, std::vector<std::string>>> ess;
	};
	std::vector<block> stk(1);
	auto find_varorconst = [&](std::string const& nam) {
		for (auto it = stk.rbegin(); it != stk.rend(); ++it) {
			auto fnd = it->varorconst.find(nam);
			if (fnd != it->varorconst.end())
				return fnd->second;
		}
		return 'X';
	};
	std::string str;
	while (pre >> str) {
		// Open/close a block
		if (str == "${") {
			stk.emplace_back();
		} else if (str == "$}") {
			if (stk.size() == 1)
				throw std::runtime_error("no block to close\n");
			stk.pop_back();
		
		// Variable/constant
		} else if (str == "$v" || str == "$c") {
			char c = str[1];
			while (pre >> str) {
				if (str == "$.")
					break;
				if (stk.back().varorconst.find(str) != stk.back().varorconst.end())
					throw std::runtime_error(str + " already exists in current scope\n");
				stk.back().varorconst.emplace(std::move(str), c);
			}
		
		// Disjoint var condition
		} else if (str == "$d") {
			std::unordered_set<std::string> arr;
			while (pre >> str) {
				if (str == "$.")
					break;
				arr.insert(std::move(str));
			}
			if (arr.size() <= 1)
				throw std::runtime_error("need at least two vars for disjoint condition\n");
			for (auto it = arr.begin(); it != arr.end(); ++it) {
				for (auto jt = std::next(it); jt != arr.end(); ++jt) {
					stk.back().disjs[*it].insert(*jt);
					stk.back().disjs[*jt].insert(*it);
				}
			}
		
		} else {
			std::string name = std::move(str);
			// Floating hypothesis
			std::string str2;
			pre >> str2;
			if (str2 == "$f") {
				std::string typecode, var;
				pre >> typecode;
				while (pre >> var) {
					if (var == "$.")
						break;
					if (find_varorconst(var) != 'v')
						throw std::runtime_error("no such variable " + var + '\n');
					if (stk.back().var2float.find(var) != stk.back().var2float.end())
						throw std::runtime_error("floating hypothesis for " + var + " already exists\n");
					stk.back().var2float[var] = typecode;
					stk.back().float2vars[typecode].insert(std::move(var));
				}
				

			// Essential hypothesis
			} else if (str2 == "$e") {
				std::vector<std::string> seq;
				std::string typecode, var;
				pre >> typecode;
				if (find_varorconst(typecode) != 'c')
					throw std::runtime_error("no such typecode " + typecode + '\n');
				while (pre >> var) {
					if (var == "$.")
						break;
					if (find_varorconst(var) == 'X')
						throw std::runtime_error("no such symbol " + var + '\n');
					seq.push_back(std::move(var));
				}
				stk.back().ess[name] = std::move(std::make_pair(std::move(typecode), std::move(seq)));
			
			} else if (str2 == "$a") {
				// WIP
			} else if (str2 == "$p") {
				// WIP
			}
		}
	}
	if (stk.size() != 1)
		throw std::runtime_error("unclosed block\n");
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
