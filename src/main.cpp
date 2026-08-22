#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <istream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#define BUFSIZ_11MM (1 << 16)
using namespace std;
using ull = unsigned long long;

unordered_set<string> incfiles;

struct fastio {
	FILE *fin;
	char buf[BUFSIZ_11MM], last = '\0', next = '\0';
	size_t pos = 0, end = 0;
	bool fail = false;
	
	fastio(FILE *f) : fin(f) {}

	bool get(char& c) {
		if (fail) return false;
		if (next) {
			c = next;
			next = '\0';
			return true;
		}
		if (pos == end) {
			end = fread(buf, 1, sizeof buf, fin);
			pos = 0;
			if (!end) {
				fail = true;
				return false;
			}
		}
		last = c = buf[pos++];
		return true;
	}

	void unget() {
		next = last;
	}
};

struct sym {
	ull id;
	enum : unsigned char {
		IN_VARIABLE, CONSTANT, VARIABLE
	} kind;

	bool operator==(sym const& rhs) const noexcept { return id == rhs.id && kind == rhs.kind; }
	bool operator!=(sym const& rhs) const noexcept { return id != rhs.id || kind != rhs.kind; }
};

struct stmt {
	vector<sym> syms;
	// For essential hypotheses only
	unordered_set<ull> vars;

	// For assertions only
	unordered_set<ull> mandvars;
	vector<stmt*> mandhyps;
	vector<pair<ull, ull>> manddisjs;

	enum : unsigned char {
		ESSENTIAL_HYP, FLOATING_HYP, ASSERTION, IN_ESSENTIAL_HYP, IN_FLOATING_HYP
	} kind;
};

struct stkclean {
	vector<reference_wrapper<sym>> syms;
	vector<reference_wrapper<stmt>> hyps;
	vector<pair<ull, ull>> disjs;
};

unordered_map<string, sym> str2sym;
unordered_map<string, stmt> str2stmt;
unordered_map<ull, unordered_map<ull, ull>> disjs;
unordered_map<ull, pair<ull, bool>> symid2type;
vector<stkclean> cleanup(1, stkclean{});
ull tokcnt, symcnt;

bool verify(fastio&& in, string const& filename = {}, bool reset = false) {
	string tok;
	bool ret = true;

	auto info = [&](string const& str) {
		cout << "[INFO] " << str << '\n';
	};
	auto warn = [&](string const& str) {
		cerr << "[WARN] " << str << '\n';
	};
	auto error = [&](string const& str) {
		cerr << "[ERROR @ token #" << tokcnt;
		if (!filename.empty())
			cerr << ", in file " << filename;
		cerr << "] " << str << '\n';
		
		if (!filename.empty())
			warn("Database " + filename + " not valid!");
		else warn("Stdin not valid!");
		in.fail = true;
		return ret = false;
	};

	// metamath doesnt have \v
	auto isspace = [](char c) {
		return c == ' ' || c == '\n' || c == '\t' || c == '\f' || c == '\r';
	};
	auto rdtok0 = [&](bool comment) {
		auto rdword = [&]() {
			char c;
			while (in.get(c))
				if (!isspace(c)) {
					in.unget();
					break;
				}
			if (in.pos >= in.end && !in.next)
				return false;

			tok.clear();
			
			while (in.get(c) && !isspace(c)) {
				if (c < '!' || c > '~')
					return error("Unprintable character " + string(1, c));
				tok += c;
			}
			return !tok.empty();
		};

		bool cmt = false;
		while (comment || (++tokcnt, rdword())) {
			if (comment || tok == "$(") {
				cmt = true;
				while (++tokcnt, rdword())
					if (tok == "$)") {
						cmt = false;
						break;
					}
				if (cmt)
					return error("Unclosed comment");
				if (comment)
					return true;
				continue;
			}
			return true;
		}
		return false;
	};
	auto rdtok = [&]() { return rdtok0(false); };
	auto rdcom = [&]() { return rdtok0(true); };

	if (reset) {
		if (!incfiles.empty())
			incfiles.clear();
		tokcnt = symcnt = 0;

		str2sym.clear();
		str2stmt.clear();
		disjs.clear();
		symid2type.clear();
		cleanup = vector<stkclean>(1, stkclean{});
	}

	if (!filename.empty())
		incfiles.insert(filename);

	while (rdtok()) {
		// File inclusion
		if (tok == "$[") {
			if (!rdtok())
				return error("Expected filename");
			auto filename2 = std::move(tok);
			if (incfiles.find(filename2) != incfiles.end())
				warn("Ignored file inclusion of " + filename2 + " from " + filename);
			else {
				if (!all_of(filename2.begin(), filename2.end(), [](char ch) { return ch != '$'; }))
					return error("Filename contains $");
				FILE *fin = fopen(filename2.c_str(), "r");
				if (!fin)
					return error("Could not open file " + filename2);
				if (!rdtok() || tok != "$]") {
					fclose(fin);
					return error("Expected end of file inclusion");
				}
				ret &= verify(fin, filename2);
				fclose(fin);
			}

		// Block
		} else if (tok == "${")
			cleanup.emplace_back();
		else if (tok == "$}") {
			if (cleanup.size() <= 1)
				return error("No block to close");

			for (auto& sym : cleanup.back().syms)
				sym.get().kind = sym::IN_VARIABLE;
			
			for (auto const& stmt : cleanup.back().hyps) {
				auto& st = stmt.get();
				if (st.kind == stmt::FLOATING_HYP) {
					st.kind = stmt::IN_FLOATING_HYP;
					symid2type.at(st.syms[1].id).second = false;
				} else stmt.get().kind = stmt::IN_ESSENTIAL_HYP;
			}
			for (auto const& disj : cleanup.back().disjs) {
				auto& set = disjs.at(disj.first);
				if (!--set.at(disj.second))
					set.erase(disj.second);
				if (set.empty())
					disjs.erase(disj.first);
			}
			cleanup.pop_back();

		// Constants
		} else if (tok == "$c") {
			for (;;) {
				if (!rdtok())
					return error("Expected end of constant statement");
				if (tok == "$.")
					break;
				if (!all_of(tok.begin(), tok.end(), [](char ch) { return ch != '$'; }))
					return error("Constant symbol " + tok + " contains $");
				auto fnd = str2sym.find(tok);
				if (fnd != str2sym.end())
					return error("Can't define constant " + tok + ", symbol already exists");
				auto fnd2 = str2stmt.find(tok);
				if (fnd2 != str2stmt.end())
					return error("Can't define constant " + tok + ", label already exists");
				str2sym.emplace(tok, sym{symcnt++, sym::CONSTANT});
			}
		// Variables
		} else if (tok == "$v") {
			for (;;) {
				if (!rdtok())
					return error("Expected end of variable statement");
				if (tok == "$.")
					break;
				if (!all_of(tok.begin(), tok.end(), [](char ch) { return ch != '$'; }))
					return error("Variable symbol " + tok + " contains $");
				auto fnd = str2sym.find(tok);
				if (fnd == str2sym.end()) {
					auto fnd2 = str2stmt.find(tok);
					if (fnd2 != str2stmt.end())
						return error("Can't define variable " + tok + ", label already exists");
					cleanup.back().syms.emplace_back(str2sym.emplace(tok, sym{symcnt++, sym::VARIABLE}).first->second);
				} else {
					if (fnd->second.kind == sym::VARIABLE || fnd->second.kind == sym::CONSTANT)
						return error("Can't define variable " + tok + ", symbol already exists");
					auto fnd2 = str2stmt.find(tok);
					if (fnd2 != str2stmt.end())
						return error("Can't define variable " + tok + ", label already exists");
					fnd->second.kind = sym::VARIABLE;
					cleanup.back().syms.emplace_back(fnd->second);
				}
			}

		// Disjoint variable conditions
		} else if (tok == "$d") {
			unordered_set<ull> varids;
			for (;;) {
				if (!rdtok())
					return error("Expected end of disjoint variable statement");
				if (tok == "$.")
					break;
				auto fnd = str2sym.find(tok);
				if (fnd == str2sym.end() || fnd->second.kind != sym::VARIABLE)
					return error("No such active variable symbol " + tok);
				if (varids.find(fnd->second.id) != varids.end())
					return error("Variable " + tok + " already in the disjoint variable condition");
				varids.insert(fnd->second.id);
			}
			for (auto it = varids.begin(); it != varids.end(); ++it)
				for (auto jt = next(it); jt != varids.end(); ++jt) {
					auto x = min(*it, *jt), y = max(*it, *jt);
					disjs[x][y]++;
					cleanup.back().disjs.emplace_back(x, y);
				}
		
		} else {
			string labstr = std::move(tok), stmtkind;
			ull typecode;
			if (str2stmt.find(labstr) != str2stmt.end())
				return error("Can't define label " + labstr + ", it already exists");
			if (str2sym.find(labstr) != str2sym.end())
				return error("Can't define label " + labstr + ", symbol already exists");
			if (!all_of(labstr.begin(), labstr.end(), [](char ch) {
				return ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9'))
				      || ch == '.' || ch == '-' || ch == '_';
			})) return error("Label " + labstr + " not sane");
			
			if (!rdtok())
				return error("Expected statement kind after label " + labstr);
			stmtkind = std::move(tok);

			if (!rdtok())
				return error("Expected typecode");
			auto fnd = str2sym.find(tok);
			if (fnd == str2sym.end())
				return error("No such active constant (for use as typecode) " + tok);
			typecode = fnd->second.id;

			// Floating hypothesis
			if (stmtkind == "$f") {
				if (!rdtok())
					return error("Expected active variable symbol in floating hypothesis");
				string var = std::move(tok);
				auto fnd = str2sym.find(var);
				if (fnd == str2sym.end() || fnd->second.kind != sym::VARIABLE)
					return error("No such active variable symbol " + tok);

				if (!rdtok() || tok != "$.")
					return error("Expected end of floating hypothesis");

				auto fnd2 = symid2type.find(fnd->second.id);
				if (fnd2 != symid2type.end()) {
					if (fnd2->second.second)
						return error("A floating hypothesis for " + var + " is already active");
					if (fnd2->second.first != typecode)
						return error("Floating hypothesis for " + var + " contradicts existing typecode");
					fnd2->second.second = true;
				} else symid2type.emplace(fnd->second.id, make_pair(typecode, true));
				
				cleanup.back().hyps.emplace_back(str2stmt.emplace(labstr, stmt{
					{{typecode, sym::CONSTANT}, fnd->second}, {}, {}, {}, {}, stmt::FLOATING_HYP
				}).first->second);

			// Essential hypothesis
			} else if (stmtkind == "$e") {
				vector<sym> seq = {{typecode, sym::CONSTANT}};
				unordered_set<ull> vars;
				for (;;) {
					if (!rdtok())
						return error("Expected end of essential hypothesis");
					if (tok == "$.")
						break;

					auto fnd = str2sym.find(tok);
					if (fnd == str2sym.end())
						return error("No such symbol " + tok);
				
					if (fnd->second.kind == sym::IN_VARIABLE)
						return error("Variable symbol " + tok + " is inactive");
					else if (fnd->second.kind == sym::VARIABLE) {
						auto fnd2 = symid2type.find(fnd->second.id);
						if (fnd2 == symid2type.end() || !fnd2->second.second)
							return error("Variable " + tok + " in essential hypothesis " + labstr + " has no active floating hypothesis (typecode)");
						vars.insert(fnd->second.id);
					}
					seq.push_back(fnd->second);
				}
				cleanup.back().hyps.emplace_back(str2stmt.emplace(labstr, stmt{
					std::move(seq), std::move(vars), {}, {}, {}, stmt::ESSENTIAL_HYP
				}).first->second);
			
			} else {
				bool p = (stmtkind == "$p"), a = (stmtkind == "$a");
				if (!p && !a)
					return error("Bad statement kind " + stmtkind);

				/* (1) */ unordered_set<ull> mandvars;
				/* (2) */ vector<stmt*> mandhyps;
				/* (3) */ vector<pair<ull, ull>> manddisjs;

				vector<sym> seq = {{typecode, sym::CONSTANT}};
				for (;;) {
					if (!rdtok())
						return error("Expected end of assertion symbol sequence");
					if ((p && tok == "$=") || (a && tok == "$."))
						break;
					auto fnd = str2sym.find(tok);
					if (fnd == str2sym.end())
						return error("No such symbol " + tok);

					if (fnd->second.kind == sym::VARIABLE) {
						auto fnd2 = symid2type.find(fnd->second.id);
						if (fnd2 == symid2type.end() || !fnd2->second.second)
							return error("Variable " + tok + " in assertion " + labstr + " has no active floating hypothesis (typecode)");
						/* (1): 1/2 */ mandvars.insert(fnd->second.id);
					
					} else if (fnd->second.kind != sym::CONSTANT)
						return error("Symbol " + tok + " in assertion " + labstr + " is not an active variable or constant");

					seq.push_back(fnd->second);
				}

				for (auto const& it : cleanup)
					for (auto const& hyp : it.hyps)
						if (hyp.get().kind == stmt::ESSENTIAL_HYP)
							/* (1): 2/2 */ mandvars.insert(hyp.get().vars.begin(), hyp.get().vars.end());

				for (auto const& it : cleanup) {
					for (auto const& hyp : it.hyps)
						if ((hyp.get().kind == stmt::ESSENTIAL_HYP)
						||  (hyp.get().kind == stmt::FLOATING_HYP && mandvars.find(hyp.get().syms[1].id) != mandvars.end()))
							/* (2): 1/2, 2/2 */ mandhyps.push_back(&hyp.get());
					for (auto const& disj : it.disjs)
						if (mandvars.find(disj.first) != mandvars.end() && mandvars.find(disj.second) != mandvars.end())
							/* (3): 1/1 */ manddisjs.emplace_back(disj);
				}

				bool proved = false;
				// Axiomatic assertion
				if (stmtkind == "$a")
					proved = true;

				// Provable assertion
				else {
					struct step {
						enum : unsigned char {
							STATEMENT, COMPRESS_COPY, COMPRESS_USE
						} type;
						ull compr_idx;
						stmt *ptr;
					};
					vector<step> steps;
					vector<vector<sym>> proof_stk, copy_stk;
					unordered_map<ull, vector<sym>> substmap;
					bool compressed = false, rightafter1st = true, noproof = false;
					if (!rdtok())
						return error("Expected end of proof in provable assertion " + labstr);
					if (tok == "(")
						compressed = true;

					while (true) {
						if ((!rightafter1st || compressed) && !rdtok()) 
							return error("Expected end of proof in provable assertion " + labstr);
						if (rightafter1st) rightafter1st = false;
						if (compressed) {
							if (tok == ")")
								break;
						} else {
							if (tok == "$.")
								break;
							if (tok == "?")
								noproof = true;
						}
						if (tok != "?") {
							auto fnd = str2stmt.find(tok);
							if (fnd == str2stmt.end() || fnd->second.kind == stmt::IN_ESSENTIAL_HYP || fnd->second.kind == stmt::IN_FLOATING_HYP)
								return error("No such active label " + tok + " in provable assertion " + labstr);
							steps.push_back(step{step::STATEMENT, 0, &fnd->second});
						}
					}
					if (compressed) {
						auto stmts = std::move(steps);
						steps.clear();
						
						bool in_ws = true, ok = false, last20 = false;
						char c;
						ull charcnt = 0, curr = 0;
						for (;;) {
							if (++charcnt, !in.get(c))
								return error("Expected end of proof in provable assertion " + labstr);
							if (in_ws) {
								if (!isspace(c)) {
									in_ws = false;
									++tokcnt;
									if (c == '$') {
										if (++charcnt, !in.get(c))
											return error("(char #" + to_string(charcnt) + ") Expected period or paren after $ in provable assertion " + labstr);
										/* empty */ {
											char exp_sp;
											if (++charcnt, in.get(exp_sp) && !isspace(exp_sp))
												return error("(char #" + to_string(charcnt) + ") Unexpected character in provable assertion " + labstr);
										}

										if (c == '.')
											break;
										if (c == '(') {
											if (!rdcom())
												return false;
											continue;
										} else return error("(char #" + to_string(charcnt) + ") Unexpected character in provable assertion " + labstr);
									}
								}
							}
							if (!in_ws) {
								if (isspace(c))
									in_ws = true;

								else {
									if (c == '?')
										noproof = true;
									else if (c < 'A' || c > 'Z')
										return error("(char #" + to_string(charcnt) + ") Expected ? or uppercase character in compressed proof string in provable assertion "
										       + labstr);
								
									ok = true;
									// this time i'll assume ascii again because metamath actually requires it + less effort
									if (c == 'Z') {
										if (!last20)
											return error("Bad Z positioning");
										steps.push_back(step{step::COMPRESS_COPY, 0, nullptr});
										last20 = false;
									
									} else if (c >= 'U' && c <= 'Y') {
										curr = curr * 5 + c - 'U' + 1;
										last20 = false;
									
									// c >= 'A' && c <= 'T'
									} else {
										curr = curr * 20 + c - 'A';
										if (curr < mandhyps.size())
											steps.push_back(step{step::STATEMENT, 0, mandhyps[curr]});
										else {
											curr -= mandhyps.size();
											if (curr < stmts.size())
												steps.push_back(stmts[curr]);
											else {
												curr -= stmts.size();
												steps.push_back(step{step::COMPRESS_USE, curr, nullptr});
											}
										}
										curr = 0;
										last20 = true;
									}
								}
							}
						}
						if (!ok)
							return error("Empty compressed proof string in provable assertion " + labstr);
						if (curr)
							return error("Bad compressed proof string in provable assertion " + labstr);
					}

					if (noproof)
						goto noproof_label;
					for (auto const& step : steps) {
						switch (step.type) {
							case step::COMPRESS_COPY:
								if (proof_stk.empty())
									return error("Empty proof stack when trying to copy in provable assertion " + labstr);
								copy_stk.push_back(proof_stk.back());
								break;
							case step::COMPRESS_USE:
								if (step.compr_idx >= copy_stk.size())
									return error("Can't repeat compressed index #" + to_string(step.compr_idx) + " in provable assertion " + labstr);
								proof_stk.push_back(copy_stk[step.compr_idx]);
								break;

							case step::STATEMENT:
								switch (step.ptr->kind) {
									case stmt::FLOATING_HYP: /* FALLTHROUGH */
									//case stmt::IN_FLOATING_HYP: /* FALLTHROUGH */
									case stmt::ESSENTIAL_HYP: /* FALLTHROUGH */
									//case stmt::IN_ESSENTIAL_HYP:
										proof_stk.push_back(step.ptr->syms);
										break;

									case stmt::ASSERTION:
										substmap.clear();

										/* empty */ {
											auto subst_seq = [&](vector<sym> const& seq) {
												vector<sym> ret;
												for (auto const& it : seq)
													if (it.kind == sym::CONSTANT)
														ret.push_back(it);
													else {
														auto fnd = substmap.find(it.id);
														if (fnd != substmap.end())
															ret.insert(ret.end(), fnd->second.begin(), fnd->second.end());
													}
												return ret;
											};

											// STEP 1: check unification and build substmap
											if (step.ptr->mandhyps.size() > proof_stk.size())
												return error("Stack underflow in provable assertion " + labstr);
											auto const& mandhyps = step.ptr->mandhyps;
											size_t base = proof_stk.size() - mandhyps.size();
											for (size_t i = 0; i < mandhyps.size(); ++i) {
												if (mandhyps[i]->kind == stmt::FLOATING_HYP || mandhyps[i]->kind == stmt::IN_FLOATING_HYP) {
													if (mandhyps[i]->syms[0] != proof_stk[base + i][0])
														return error("Couldn't unify (typecodes don't match) in provable assertion " + labstr);
													
													auto& map = substmap[mandhyps[i]->syms[1].id];
													map = proof_stk[base + i];
													map.erase(map.begin());

												} else if ((mandhyps[i]->kind == stmt::ESSENTIAL_HYP || mandhyps[i]->kind == stmt::IN_ESSENTIAL_HYP)
												       &&   subst_seq(mandhyps[i]->syms) != proof_stk[base + i])
													return error("Couldn't unify in provable assertion " + labstr);
											}

											// STEP 2: get rid of $e and $f
											proof_stk.resize(base);

											// STEP 3: verify disjoint variable conditions
											for (auto const& it : step.ptr->manddisjs) {
												auto x = it.first, y = it.second;
												auto f1 = substmap.find(x), f2 = substmap.find(y);
												if (f1 == substmap.end() || f2 == substmap.end())
													return error("UNREACHABLE!");
												for (auto const& a : f1->second) {
													if (a.kind != sym::VARIABLE)
														continue;
													for (auto const& b : f2->second) {
														if (b.kind != sym::VARIABLE)
															continue;
														if (a.id == b.id)
															return error("Disjoint variable condition violation (two same variables) in provable assertion " + labstr);
														
														auto A = min(a.id, b.id), B = max(a.id, b.id);
														bool ok = false;
														auto fnd2 = disjs.find(A);
														if (fnd2 != disjs.end()) {
															auto fnd3 = fnd2->second.find(B);
															if (fnd3 != fnd2->second.end())
																ok = true;
														}
														if (!ok)
															return error("Disjoint variable condition violation (two variables mapped in the substitution map by variables "
															             "in a disjoint variable condition aren't in a disjoint variable condition in provable assertion "
															             + labstr);
													}
												}
											}

											// STEP 4: push the substitued assertion sequence back onto the stack
											proof_stk.push_back(subst_seq(step.ptr->syms));
										}
										break;

									default:
										return error("Referenced proof step is not active");
								}
								break;
						}
					}
					if (proof_stk.size() != 1)
						return error("Proof of assertion " + labstr + " should end with one element on the stack");
					if (proof_stk.front() != seq)
						return error("Proof of assertion " + labstr + " doesn't prove the correct statement");

					info("Theorem " + labstr + " OK!");
					noproof_label:
					if (noproof)
						warn("Theorem " + labstr + " assumed to be true");
					proved = true;
				}

				if (!proved)
					return error("Theorem " + labstr + " not proved!");
				else str2stmt.emplace(labstr, stmt{
					std::move(seq), {}, std::move(mandvars), std::move(mandhyps), std::move(manddisjs), stmt::ASSERTION
				});
			}
		}
	}
	if (ret) {
		if (filename.empty())
			info("Stdin OK!");
		else info("Database " + filename + " OK!");
	}
	return ret;
}

int main(int argc, char **argv) {
	ios_base::sync_with_stdio(false);
	cin.tie(nullptr);

	bool ret = true;
	if (argc == 1)
		ret = verify(stdin);
	else for (int i = 1; i < argc; ++i) {
		bool v;
		if (!strcmp(argv[i], "-"))
			v = verify(stdin, {}, true);
		else {
			FILE *fin = fopen(argv[i], "r");
			if (!fin) {
				perror(argv[i]);
				continue;
			}
			v = verify(fin, argv[i], true);
			fclose(fin);
		}
		ret &= v;
	}
	return ret ? EXIT_SUCCESS : EXIT_FAILURE;
}
