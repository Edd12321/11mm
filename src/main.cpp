#include <cctype>
#include <cerrno>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <iostream>
#include <istream>
#include <limits>
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
		std::size_t lc = 1, cc = 0;
		stream_ptr() = delete;
		stream_ptr(std::istream& p) : ptr(&p) {}
		stream_ptr(std::unique_ptr<std::ifstream>&& f, std::string fname)
				: ptr(f.get()), file(std::move(f)), filename(std::move(fname)) {}
	};
	std::deque<stream_ptr> stk;
	std::size_t& lc() { return stk.back().lc; }
	std::size_t& cc() { return stk.back().cc; }

	bool read(std::string& str) {
		if (stk.empty())
			error("can't read\n");
		auto *in = &stk.back();
		// while (!(*(in->ptr) >> str)) {
		bool ok = false;
		char c;
		enum class states {
			BEFORE_TOK,
			TOK,
		} state;
		for (;;) {
			state = states::BEFORE_TOK;
			bool brk = false, cleared = false;
			while (!brk) {
				switch (state) {
					case states::BEFORE_TOK:
						if (!in->ptr->get(c)) {
							brk = true;
							break; 
						}
						if (!cleared) {
							cleared = true;
							str.clear();
						}
						++cc();
						if (!std::isspace(static_cast<unsigned char>(c)))
							ok = true, state = states::TOK;
						else if (c == '\n')
							++lc(), cc() = 0;
						break;

					case states::TOK:
						/* empty */ {
							str += c;
							auto d = in->ptr->peek();
							if (d == std::char_traits<char>::eof()
							||  std::isspace(static_cast<unsigned char>(d))
							||  !(in->ptr->get(c))) {
									brk = true;
									break;
							}
							++cc();
						}
						break;
				}
			}
			if (ok)
				break;
			if (!in->ptr->eof())
				error("read error\n");
			stk.pop_back();
			if (stk.empty())
				return false;
			in = &stk.back();
		}
		return true;
	}
public:
	std::size_t const& lc() const { return stk.back().lc; }
	std::size_t const& cc() const { return stk.back().cc; }
	template<typename T>
	void error(T const& msg, bool fatal = true) {
		std::string info = "ERROR ";
		if (!stk.empty()) {
			if (!stk.back().filename.empty())
				info += "In file " + stk.back().filename + ": ";
			info += std::to_string(lc()) + ':' + std::to_string(cc()) + ", ";
		}
		if (fatal)
			throw std::runtime_error(info + msg);
		std::cerr << info << msg << '\n';
	}

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
			error(file + ": " + std::strerror(errno));
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
					error("no filename\n");
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
						error(str + ": " + std::strerror(errno));
					std::string brk;
					if (!read(brk) || brk != "$]")
						error("expected end of file inclusion\n");
					stk.emplace_back(std::move(fin), str);
				} else {
					// Silent ignore if it appeared once
					if (!read(str) || str != "$]")
						error("expected end of file inclusion\n");
				}
			} else break;
		}
		return *this;
	}
};

void eval(preprocessor& pre) {
	// Math symbols (variables && constants)
	using symid = unsigned int;
	struct mathsym {
		symid id;
		enum class skind {
			VARIABLE,
			CONSTANT
		} kind;
	};
	std::unordered_map<std::string, mathsym> str2sym;
	std::unordered_map<symid, symid> sym2type;
	auto symc = std::numeric_limits<symid>::min();
	// Statements (hypotheses && assertions)
	struct stmt {
		std::vector<mathsym> seq;
		enum class stmtkind {
			FLOATING,
			ESSENTIAL,
			AXIOM,
			PROVEABLE
		} kind;
		std::unordered_set<symid> mandvars;
		std::vector<stmt> mandhyps;
		std::unordered_map<symid, std::unordered_set<symid>> manddisjs;
	};
	struct stmtref {
		std::size_t stk_idx, hyps_idx; // HYPOTHESIS
		std::size_t ass_idx;           // ASSUMPTION
		enum class refkind {
			ASSUMPTION,
			HYPOTHESIS
		} kind;
	};
	using label = unsigned int;
	std::unordered_map<std::string, label> str2label;
	std::unordered_map<label, stmtref> label2ref;
	std::vector<stmt> stmts;
	auto labc = std::numeric_limits<label>::min();

	// Blocks
	struct block {
		// Variables
		std::unordered_set<symid> vars;
		// Disjoint variable conditions
		std::unordered_map<symid, std::unordered_set<symid>> disjs;
		// Floating && essential hypothesis
		std::vector<stmt> hyps;
	};
	std::vector<block> stk(1);
	
	std::string str;
	while (pre >> str) {
		//
		// Scoping statement
		//
		if (str == "${") {
			stk.emplace_back();
		} else if (str == "$}") {
			if (stk.size() == 1)
				pre.error("can't close block here");
			stk.pop_back();

		//
		// Constant math symbol
		//
		} else if (str == "$c") {
			if (stk.size() != 1)
				pre.error("constant statement outside outermost block");
			while (pre >> str) {
				if (str == "$.")
					break;
				auto fnd = str2sym.find(str);
				if (fnd == str2sym.end())
					str2sym[std::move(str)] = {symc++, mathsym::skind::CONSTANT};
				else
					pre.error("symbol " + str + " already exists");
			}

		//
		// Variable math symbol
		//
		} else if (str == "$v") {
			while (pre >> str) {
				if (str == "$.")
					break;
				auto fnd = str2sym.find(str);
				if (fnd == str2sym.end())
					str2sym[std::move(str)] = {symc++, mathsym::skind::VARIABLE};
				else {
					if (fnd->second.kind == mathsym::skind::CONSTANT)
						pre.error("symbol " + str + " already exists as a constant");
					// == VARIABLE
					for (auto const& it : stk)
						if (it.vars.find(fnd->second.id) != it.vars.end())
							pre.error("symbol " + str + " is already a variable");
					stk.back().vars.emplace(fnd->second.id);
				}
			}
		
		//
		// Disjoint variable condition
		//
		} else if (str == "$d") {
			std::unordered_set<symid> vars;
			while (pre >> str) {
				if (str == "$.")
					break;
				auto fnd = str2sym.find(str);
				if (fnd == str2sym.end())
					pre.error("symbol " + str + " not found");
				if (fnd->second.kind != mathsym::skind::VARIABLE)
					pre.error("symbol " + str + " is not a variable, but a constant");
				vars.emplace(fnd->second.id);
			}
			for (auto it = vars.begin(); it != vars.end(); ++it)
				for (auto jt = std::next(it); jt != vars.end(); ++jt)
					stk.back().disjs[std::min(*it, *jt)].insert(std::max(*it, *jt));
		
		} else {
			std::string labstr = std::move(str), typestr;
			// check label sanity
			for (auto const& ch : labstr)
				if (!std::isalnum(static_cast<unsigned char>(ch))
				&&  ch != '.' && ch != '-' && ch != '_')
					pre.error(labstr + " is not a valid label");
			if (str2label.find(labstr) != str2label.end())
				pre.error("label " + labstr + " was aleady used once");
			str2label[labstr] = labc;

			pre >> str >> typestr;
			auto fnd = str2sym.find(typestr);
			if (fnd == str2sym.end())
				pre.error("symbol " + typestr + " not found");
			if (fnd->second.kind != mathsym::skind::CONSTANT)
				pre.error("symbol " + typestr + " is not a constant (typecode), but a variable");
			auto const& typecode = fnd->second;
		
			//
			// Floating hypothesis
			//
			if (str == "$f") {
				std::string var;
				pre >> var;
				auto fnd = str2sym.find(var);
				if (fnd == str2sym.end())
					pre.error("symbol " + var + " not found");
				if (fnd->second.kind != mathsym::skind::VARIABLE)
					pre.error("symbol " + var + " is not a variable, but a constant");
				auto const& varsym = fnd->second;
				
				auto fnd2 = sym2type.find(varsym.id);
				if (fnd2 != sym2type.end() && fnd2->second != typecode.id)
					pre.error("type " + typestr + " is inconsistent with earlier floating hypotheses");

				pre >> str;
				if (str != "$.")
					pre.error("expected end of floating hypothesis");

				std::vector<mathsym> vec = { typecode, varsym };
				stk.back().hyps.push_back({
					std::move(vec),          // .seq
					stmt::stmtkind::FLOATING // .kind
				});
				label2ref[labc] = {
					stk.size() - 1,              // .stk_idx
					stk.back().hyps.size() - 1,  // .hyps_idx
					0,                           // .ass_idx
					stmtref::refkind::HYPOTHESIS // .kind
				};
				sym2type[varsym.id] = typecode.id;
	
			//
			// Essential hypothesis
			//
			} else if (str == "$e") {
				std::string var;
				std::vector<mathsym> vec = { typecode };
				while (pre >> var) {
					if (var == "$.")
						break;
					auto fnd = str2sym.find(var);
					if (fnd == str2sym.end())
						pre.error("symbol " + var + " not found");
					vec.push_back(fnd->second);
				}
				stk.back().hyps.push_back({
					std::move(vec),            // .seq
					stmt::stmtkind::ESSENTIAL  // .kind
				});
				label2ref[labc] = {
					stk.size() - 1,              // .stk_idx
					stk.back().hyps.size() - 1,  // .hyps_idx
					0,                           // .ass_idx
					stmtref::refkind::HYPOTHESIS // .kind
				};
			
			} else {
				std::string stopstr, var;
				if (str == "$a")
					stopstr = "$.";
				else if (str == "$p")
					stopstr = "$=";
				else pre.error("invalid statement " + str);
				std::vector<mathsym> seq = { typecode };
				while (pre >> var) {
					if (var == stopstr)
						break;
					auto fnd = str2sym.find(var);
					if (fnd == str2sym.end())
						pre.error("symbol " + var + " not found");
					seq.push_back(fnd->second);
				}

				// 1) mandatory variables == variables appearing in the sequence of
				// math symbols together with the variables appearing in all
				// essential hypotheses thus far
				std::unordered_set<symid> mandvars;

				// 2) mandatory hypotheses == essential hypotheses together with
				// all floating hypotheses containing mandatory variables
				std::vector<stmt> mandhyps;
		
				// 3) mandatory disjoint variable condition == disjoint variable
				// condition where both variables are mandatory variables
				std::unordered_map<symid, std::unordered_set<symid>> manddisjs;

				for (auto const& w : seq)
					if (w.kind == mathsym::skind::VARIABLE)
						/* 1) */ mandvars.insert(w.id);
				
				for (auto const& it : stk) {
					for (auto const& hyp : it.hyps) {
						switch (hyp.kind) {
							case stmt::stmtkind::ESSENTIAL:
								for (auto const& w : hyp.seq)
									if (w.kind == mathsym::skind::VARIABLE)
										/* 1) */ mandvars.insert(w.id);
								
								/* 2) */ mandhyps.emplace_back(hyp);
								break;

							case stmt::stmtkind::FLOATING:
								if (mandvars.find(hyp.seq[1].id) != mandvars.end())
									/* 2) */ mandhyps.emplace_back(hyp);
								break;

							default:
								pre.error("UNREACHABLE!");
								break;
						}
					}
					for (auto const& dc : it.disjs)
						if (mandvars.find(dc.first) != mandvars.end())
							for (auto const& var2 : dc.second)
								if (mandvars.find(var2) != mandvars.end())
									/* 3) */ manddisjs[dc.first].insert(var2);

				}


				//
				// Axiomatic assertion
				//
				if (str == "$a") {
					stmts.push_back({
						std::move(seq),        // .seq
						stmt::stmtkind::AXIOM, // .kind
						std::move(mandvars),   // .mandvars
						std::move(mandhyps),   // .mandhyps
						std::move(manddisjs)   // .manddisjs
					});

				//
				// Proveable assertion
				//
				} else if (str == "$p") {
					// WIP
				}
			}
			++labc;
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
