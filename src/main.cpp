#include <algorithm>
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
		std::string info = "[ERROR] ";
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

	operator bool() const {
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
		bool operator==(mathsym const& rhs) const {
			return id == rhs.id && kind == rhs.kind;
		}
		bool operator!=(mathsym const& rhs) const {
			return id != rhs.id || kind != rhs.kind;
		}
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
		std::size_t ass_idx;           // ASSERTION
		enum class refkind {
			ASSERTION,
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


		std::unordered_set<label> labels_to_del;
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
			for (auto const& l : stk.back().labels_to_del)
				label2ref.erase(l);
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
				stk.back().labels_to_del.insert(labc);
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
				stk.back().labels_to_del.insert(labc);
			
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
				for (auto const& it : stk)
					for (auto const& hyp : it.hyps)
						if (hyp.kind == stmt::stmtkind::ESSENTIAL)
							for (auto const& w : hyp.seq)
								if (w.kind == mathsym::skind::VARIABLE)
									/* 1) */ mandvars.insert(w.id);

				//std::vector<stmt> mande, mandf;
				for (auto const& it : stk) {
					for (auto const& hyp : it.hyps) {
						if (hyp.kind == stmt::stmtkind::ESSENTIAL)
							/* 2) */ mandhyps.push_back(hyp);
						else if (mandvars.find(hyp.seq[1].id) != mandvars.end())
							/* 2) */ mandhyps.push_back(hyp);
					}
					for (auto const& dc : it.disjs)
						if (mandvars.find(dc.first) != mandvars.end())
							for (auto const& var2 : dc.second)
								if (mandvars.find(var2) != mandvars.end())
									/* 3) */ manddisjs[std::min(dc.first, var2)].insert(std::max(dc.first, var2));
				}
				//for (auto& it : mandf) mandhyps.push_back(std::move(it));
				//for (auto& it : mande) mandhyps.push_back(std::move(it));

				// the book is very stupid and unclear about this:
				// if you wanna use a $p or $a statement, it MUST have a frame
				// that means in this part of the code:
				//
				// 1) each variable in mandvars must have an active $f
				// 2) no two $f statements contain the same variable
				// 3) the $f of any variable must be before its $e
				std::unordered_set<symid> floats;
				for (auto const& hyp : mandhyps) {
					if (hyp.kind == stmt::stmtkind::FLOATING) {
						auto varid = hyp.seq[1].id;
						if (floats.find(varid) != floats.end())
							/* 2) */ pre.error("two floating hypotheses in the associated "
							                   "frame share the same variable");
						floats.insert(varid);
					
					} else if (hyp.kind == stmt::stmtkind::ESSENTIAL) {
						for (auto const& sym : hyp.seq)
							if (sym.kind == mathsym::skind::VARIABLE && floats.find(sym.id) == floats.end())
								/* 3) */ pre.error("essential hypothesis in the associated "
								                   "frame doesn't have a floating hypothesis before it");
					}
				}
				if (floats.size() != mandvars.size())
					/* 1) */ pre.error("not every mandatory variable has an associated floating hypothesis");

				bool proved = false;
				
				//
				// Axiomatic assertion
				//
				if (str == "$a") {
					proved = true;

				//
				// Proveable assertion
				//
				} else if (str == "$p") {
					std::vector<stmtref> steps;
					std::vector<std::vector<mathsym>> proof_stk;
					bool compressed = false;

					pre >> var;
					if (var == "$.")
						pre.error("premature end of proof");
					if (var == "(")
						compressed = true;

					do {
						if (var == "$.") {
							if (compressed)
								pre.error("premature end of compressed proof");
							break;
						}
						if (compressed && var == ")")
							break;
						auto fnd = str2label.find(var);
						if (fnd == str2label.end())
							pre.error("label " + var + " not found");
						auto fnd2 = label2ref.find(fnd->second);
						if (fnd2 == label2ref.end())
							pre.error("label " + var + " not valid anymore");
						steps.emplace_back(fnd2->second);
					} while (pre >> var);

					if (compressed) {
						std::vector<stmtref> list = std::move(steps);
						std::string bigstr;
						steps.clear();
						while (pre >> var) {
							if (var == "$.")
								break;
							bigstr += var;	
						}
						if (bigstr.empty())
							pre.error("empty compressed proof");

						pre.error("compressed format is a WIP");
						// wip
					}

					for (auto const& step : steps) {
						switch (step.kind) {
							case stmtref::refkind::HYPOTHESIS:
								proof_stk.push_back(stk[step.stk_idx].hyps[step.hyps_idx].seq);
								break;
							
							case stmtref::refkind::ASSERTION:
								/* empty */ {
									auto const& ass = stmts[step.ass_idx];
									std::unordered_map<symid, std::vector<mathsym>> substmap;
									
									auto subst_ass_seq = [&](std::vector<mathsym> const& ass_seq) {
										std::vector<mathsym> subst_seq;
										for (auto const& it : ass_seq) {
											auto fnd = substmap.find(it.id);
											if (fnd != substmap.end())
												for (auto const& jt : fnd->second)
													subst_seq.push_back(jt);
											else subst_seq.push_back(it);
										}
										return subst_seq;
									};

									// step 1: check unification && build substitution map
									if (ass.mandhyps.size() > proof_stk.size())
										pre.error("stack underflow");
									
									std::size_t base = proof_stk.size() - ass.mandhyps.size();
									for (std::size_t i = 0; i < ass.mandhyps.size(); ++i) {
										auto const& hyp = ass.mandhyps[i];

										if (hyp.kind == stmt::stmtkind::FLOATING) {
											if (hyp.seq[0] != proof_stk[base + i][0])
												pre.error("couldn't unify (typecodes don't match)");

											auto& map = substmap[hyp.seq[1].id];
											map = proof_stk[base + i];
											map.erase(map.begin());

										} else if (hyp.kind == stmt::stmtkind::ESSENTIAL
										       &&  subst_ass_seq(ass.seq) != proof_stk[base + i])
											pre.error("couldn't unify");
									}

									// step 2: now get rid of the $e and $f
									proof_stk.resize(base);
								
									// step 3: verify disjoint variable conditions
									//
									// 1) if two variables in the substitution map exist in a mandatory
									// hypothesis' mandatory disjoint variable statement, their
									// corresponding sequences that they map to should have no variables
									// in common
									//
									// 2) each pair (a, b) of variables from the two sequences must
									// exist in an active disjoint variable statement of the proof
									// (i. e. any $d a b $. before our $p)
									for (auto const& v1 : ass.manddisjs) {
										for (auto const& v2 : v1.second) {
											auto f1 = substmap.find(v1.first);
											auto f2 = substmap.find(v2);

											for (auto const& x : f1->second) {
												if (x.kind != mathsym::skind::VARIABLE)
													continue;
												for (auto const& y : f2->second) {
													if (y.kind != mathsym::skind::VARIABLE)
														continue;

													if (x == y)
														/* 1) */ pre.error("disjoint variable condition violation");

													auto X = std::min(x.id, y.id);
													auto Y = std::max(x.id, y.id);

													bool ok = false;
													for (auto const& it : stk) {
														auto fnd1 = it.disjs.find(X);
														if (fnd1 != it.disjs.end())
															if (fnd1->second.find(Y) != fnd1->second.end())
																ok = true;
													}
													if (!ok)
														/* 2) */ pre.error("disjoint variable condition violation");
												}
											}
										}
									}

									// step 4: push the substituted assertion sequence back onto the stack
									proof_stk.push_back(subst_ass_seq(ass.seq));
								}
								break;
						}
					}
					if (proof_stk.size() != 1)
						pre.error("proof of " + labstr + " should end with one element on the stack");
					if (proof_stk[0] != seq)
						pre.error("proof of " + labstr + " doesn't prove the correct statement");

					std::cerr << "[INFO] theorem " << labstr << " is OK!\n";
					proved = true;
				
				} else pre.error("bad statement " + str);

				if (!proved) {
					pre.error("theorem " + labstr + " not proved!");

				} else {
					stmts.push_back({
						std::move(seq),                  // .seq
						str == "$a"
							? stmt::stmtkind::AXIOM
							: stmt::stmtkind::PROVEABLE, // .kind
						std::move(mandvars),             // .mandvars
						std::move(mandhyps),             // .mandhyps
						std::move(manddisjs)             // .manddisjs
					});
					label2ref[labc] = {
						0,                           // .stk_idx
						0,                           // .hyps_idx
						stmts.size() - 1,            // .ass_idx
						stmtref::refkind::ASSERTION  // .kind
					};
				}
			}
			++labc;
		}
	}
}

int main(int argc, char **argv) {
	try {
		if (argc == 1) {
			preprocessor pre(std::cin);
			eval(pre);
		} else for (int i = 1; i < argc; ++i) {
			preprocessor pre(argv[i]);
			eval(pre);
		}
	} catch (std::runtime_error const& err) {
		std::cerr << err.what() << '\n';
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
