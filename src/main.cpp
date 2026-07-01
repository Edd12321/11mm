#include <cerrno>
#include <cstddef>
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
		std::unordered_map<std::string, std::string> var2float, name2float;
		std::unordered_map<std::string, std::unordered_set<std::string>> float2vars;
		// Essential hypothesis
		std::unordered_map<std::string, std::pair<std::string, std::vector<std::string>>> ess;
	};
	std::unordered_map<std::string, std::tuple<block, std::string, std::vector<std::string>>> ax, thm;
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
		//
		// Open/close a block
		//
		if (str == "${") {
			stk.emplace_back();
		} else if (str == "$}") {
			if (stk.size() == 1)
				throw std::runtime_error("no block to close\n");
			stk.pop_back();
		
		//
		// Variable/constant
		//
		} else if (str == "$v" || str == "$c") {
			char c = str[1];
			while (pre >> str) {
				if (str == "$.")
					break;
				if (stk.back().varorconst.find(str) != stk.back().varorconst.end())
					throw std::runtime_error(str + " already exists in current scope\n");
				stk.back().varorconst.emplace(std::move(str), c);
			}
		
		//
		// Disjoint var condition
		//
		} else if (str == "$d") {
			std::unordered_set<std::string> arr;
			while (pre >> str) {
				if (str == "$.")
					break;
				if (find_varorconst(str) != 'v')
					throw std::runtime_error(str + " not a variable\n");
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
			std::string typecode, name = std::move(str);
			//
			// Floating hypothesis
			//
			std::string str2;
			pre >> str2 >> typecode;
		
			if (find_varorconst(typecode) != 'c')
				throw std::runtime_error("no such typecode " + typecode + '\n');
			
			if (stk.back().name2float.find(name) != stk.back().name2float.end()
			||  stk.back().ess.find(name) != stk.back().ess.end())
				throw std::runtime_error("label " + name + " already exists in scope\n");

			if (str2 == "$f") {
				std::string var;
				while (pre >> var) {
					if (var == "$.")
						break;
					if (find_varorconst(var) != 'v')
						throw std::runtime_error("no such variable " + var + '\n');
					if (stk.back().var2float.find(var) != stk.back().var2float.end())
						throw std::runtime_error("floating hypothesis for " + var + " already exists\n");
					stk.back().name2float[name] = typecode;
					stk.back().var2float[var] = typecode;
					stk.back().float2vars[typecode].insert(std::move(var));
				}
				
			//
			// Essential hypothesis
			//
			} else if (str2 == "$e") {
				std::vector<std::string> seq;
				while (pre >> str) {
					if (str == "$.")
						break;
					if (find_varorconst(str) == 'X')
						throw std::runtime_error("no such symbol " + str + '\n');
					seq.push_back(std::move(str));
				}
				stk.back().ess[name] = std::move(std::make_pair(
					std::move(typecode),
					std::move(seq)));
		
			} else {
				std::string stopstr;
				if (str2 == "$a")
					stopstr = "$.";
				else if (str2 == "$p")
					stopstr = "$=";
				else throw std::runtime_error("bad token " + str2 + '\n');
				
				std::vector<std::string> seq;
				std::unordered_set<std::string> strs;
				std::unordered_map<std::string, std::ptrdiff_t> idx;
				block b;
				while (pre >> str) {
					if (str == stopstr)
						break;
					seq.push_back(str);
					strs.insert(std::move(str));
				}
				for (auto const& it : stk)
					for (auto const& ess_it : it.ess)
						for (auto const& str_it : ess_it.second.second)
							strs.insert(str_it);
				// we need:
				// 1) b.ess
				// 2) b.varorconst
				// 3) b.disjs
				// 4) b.var2float
				// 5) b.float2vars
				for (auto const& str : strs) {
					for (std::ptrdiff_t i = stk.size() - 1; i >= 0; --i) {
						auto fnd = stk[i].varorconst.find(str);
						if (fnd != stk[i].varorconst.end()) {
							// 2) b.varorconst
							b.varorconst[str] = fnd->second;
							idx[str] = i + 1; // not zero indexed
							break;
						}
					}
				}
				for (std::ptrdiff_t i = stk.size() - 1; i >= 0; --i) {
					// 1) b.ess
					for (auto const& ess_it : stk[i].ess)
						if (b.ess.find(ess_it.first) == b.ess.end())
							b.ess[ess_it.first] = ess_it.second;
					
					// 3) b.disjs
					for (auto const& disj_it : stk[i].disjs) {
						auto const& var1 = disj_it.first;
						auto const& var2s = disj_it.second;

						auto fnd = idx.find(var1);
						if (fnd != idx.end() && fnd->second <= i + 1) {
							for (auto const& var2 : var2s) {
								auto fnd = idx.find(var2);
								if (fnd != idx.end() && fnd->second <= i + 1) {
									b.disjs[var1].insert(var2);
									b.disjs[var2].insert(var1);
								}
							}
						}
					}
					
					// 4) b.var2float && 5) b.float2vars
					for (auto const& float_it : stk[i].var2float) {
						auto const& var = float_it.first;
						auto const& flt = float_it.second;

						auto fnd_idx = idx.find(var);
						if (fnd_idx != idx.end() && fnd_idx->second <= i + 1
						&&  b.var2float.find(var) == b.var2float.end()) {
								b.var2float.emplace(var, flt);
								b.float2vars[flt].insert(var);
						}
					}
				}

				//
				// Assertion
				//
				if (str2 == "$a") {
					ax.emplace(name, std::move(std::make_tuple(
						std::move(b),
						std::move(typecode),
						std::move(seq))));

				//
				// Proveable theorem
				//
				} else if (str2 == "$p") {
					// WIP
				}
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
