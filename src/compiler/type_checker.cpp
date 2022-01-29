// type_checker.cpp
#include "type_checker.h"
#include "compiler_utils.h"
#include "utilities/ykfunction.h"
using namespace yaksha;
type_checker::type_checker() = default;
type_checker::~type_checker() = default;
void type_checker::visit_assign_expr(assign_expr *obj) {
  obj->right_->accept(this);
  auto rhs = pop();
  auto name = prefix(obj->name_->token_);
  if (!scope_.is_defined(name)) {
    error(obj->name_, "This is not defined");
    return;
  }
  auto object = scope_.get(name);
  if (rhs.object_type_ != object.object_type_) {
    error(obj->name_, "Cannot assign between 2 different data types.");
  }
}
void type_checker::visit_binary_expr(binary_expr *obj) {
  auto oper = obj->opr_->type_;
  obj->left_->accept(this);
  auto lhs = pop();
  obj->right_->accept(this);
  auto rhs = pop();
  if (lhs.object_type_ != rhs.object_type_) {
    error(obj->opr_,
          "Binary operation between two different data types is not supported");
  }
  if ((oper == token_type::PLUS) && (rhs.object_type_ != object_type::DOUBLE &&
                                     rhs.object_type_ != object_type::INTEGER &&
                                     rhs.object_type_ != object_type::STRING)) {
    error(obj->opr_, "Unsupported operation");
  }
  if ((oper == token_type::SUB || oper == token_type::MUL ||
       oper == token_type::DIV || oper == token_type::GREAT ||
       oper == token_type::GREAT_EQ || oper == token_type::LESS ||
       oper == token_type::LESS_EQ) &&
      (rhs.object_type_ != object_type::DOUBLE &&
       rhs.object_type_ != object_type::INTEGER)) {
    error(obj->opr_, "Unsupported operation");
  }
  push(rhs);
}
void type_checker::visit_fncall_expr(fncall_expr *obj) {
  obj->name_->accept(this);
  auto name = pop();
  if (name.object_type_ != object_type::FUNCTION) {
    error(obj->paren_token_, "Calling a non callable "
                             "or a non existing function");
    push(ykobject());// Push None here
    return;
  }
  std::vector<ykobject> arguments{};
  for (auto arg : obj->args_) {
    arg->accept(this);
    arguments.push_back(pop());
  }
  // check if it's same size
  auto funct = functions_.get(name.string_val_);
  if (funct->params_.size() != arguments.size()) {
    error(obj->paren_token_, "Too few or too "
                             "much arguments for function call");
    push(ykobject());// Push None here
    return;
  }
  for (auto i = 0; i < funct->params_.size(); i++) {
    auto param = funct->params_[i];
    auto arg = arguments[i];
    if (!match_data_type(param.data_type_->name_, arg)) {
      std::stringstream message{};
      message << "Parameter & argument " << (i + 1) << " mismatches";
      error(obj->paren_token_, message.str());
    }
  }
  auto data = ykobject();
  data.object_type_ = convert_data_type(funct->return_type_->name_);
  push(data);
}
void type_checker::visit_grouping_expr(grouping_expr *obj) {
  obj->expression_->accept(this);
  auto inside = pop();
  push(inside);
}
void type_checker::visit_literal_expr(literal_expr *obj) {
  auto data = ykobject();
  auto literal_type = obj->literal_token_->type_;
  if (literal_type == token_type::STRING ||
      literal_type == token_type::THREE_QUOTE_STRING) {
    data.object_type_ = object_type::STRING;
  } else if (literal_type == token_type::KEYWORD_TRUE ||
             literal_type == token_type::KEYWORD_FALSE) {
    data.object_type_ = object_type::BOOL;
  } else if (literal_type == token_type::INTEGER_BIN ||
             literal_type == token_type::INTEGER_OCT ||
             literal_type == token_type::INTEGER_DECIMAL ||
             literal_type == token_type::INTEGER_HEX) {
    data.object_type_ = object_type::INTEGER;
  } else if (literal_type == token_type::FLOAT_NUMBER) {
    data.object_type_ = object_type::DOUBLE;
  }// else - none data type by default
  push(data);
}
void type_checker::visit_logical_expr(logical_expr *obj) {
  obj->left_->accept(this);
  auto lhs = pop();
  obj->right_->accept(this);
  auto rhs = pop();
  if (rhs.object_type_ != object_type::BOOL ||
      lhs.object_type_ != object_type::BOOL) {
    error(obj->opr_, "Both LHS and RHS of logical"
                     " operator need to be boolean");
  }
}
void type_checker::visit_unary_expr(unary_expr *obj) {
  // -5 - correct, -"some string" is not
  obj->right_->accept(this);
  auto rhs = pop();
  if (rhs.object_type_ != object_type::INTEGER &&
      rhs.object_type_ != object_type::DOUBLE) {
    error(obj->opr_, "Invalid unary operation");
  }
  push(rhs);
}
void type_checker::visit_variable_expr(variable_expr *obj) {
  auto name = prefix(obj->name_->token_);
  if (!scope_.is_defined(name)) {
    error(obj->name_, "Undefined name");
    push(ykobject());
    return;
  }
  auto value = scope_.get(name);
  // Preserve function name so we can access it
  if (value.object_type_ == object_type::FUNCTION) { value.string_val_ = name; }
  push(value);
}
void type_checker::visit_block_stmt(block_stmt *obj) {
  for (auto stm : obj->statements_) { stm->accept(this); }
}
void type_checker::visit_break_stmt(break_stmt *obj) {
  if (peek_scope_type() != ast_type::STMT_WHILE) {
    error(obj->break_token_,
          "Invalid use of break statement outside of while statement.");
  }
}
void type_checker::visit_continue_stmt(continue_stmt *obj) {
  if (peek_scope_type() != ast_type::STMT_WHILE) {
    error(obj->continue_token_, "Invalid use of continue"
                                " statement outside of while statement.");
  }
}
void type_checker::visit_def_stmt(def_stmt *obj) {
  // WHY? This is so I can know I am in a function when I'm in a block statement
  push_scope_type(ast_type::STMT_DEF);
  push_function(prefix(obj->name_->token_));
  scope_.push();
  for (auto param : obj->params_) {
    auto name = prefix(param.name_->token_);
    if (scope_.is_defined(name)) {
      error(param.name_, "Parameter shadows outer scope name.");
    } else {
      auto data = ykobject();
      data.object_type_ = convert_data_type(param.data_type_->name_);
      scope_.define(name, data);
    }
  }
  obj->function_body_->accept(this);
  scope_.pop();
  pop_scope_type();
  pop_function();
}
void type_checker::visit_expression_stmt(expression_stmt *obj) {
  obj->expression_->accept(this);
}
void type_checker::visit_if_stmt(if_stmt *obj) {
  obj->expression_->accept(this);
  auto bool_expression = pop();
  if (bool_expression.object_type_ != object_type::BOOL) {
    error(obj->if_keyword_, "Invalid boolean expression used");
  }
  scope_.push();
  obj->if_branch_->accept(this);
  scope_.pop();
  if (obj->else_branch_ != nullptr) {
    scope_.push();
    obj->else_branch_->accept(this);
    scope_.pop();
  }
}
void type_checker::visit_let_stmt(let_stmt *obj) {
  auto name = prefix(obj->name_->token_);
  auto placeholder = ykobject();
  placeholder.object_type_ = convert_data_type(obj->data_type_->name_);
  if (obj->expression_ != nullptr) {
    obj->expression_->accept(this);
    auto expression_data = pop();
    if (expression_data.object_type_ != placeholder.object_type_) {
      error(obj->name_, "Data type mismatch in expression and declaration.");
    }
  }
  scope_.define(name, placeholder);
}
void type_checker::visit_pass_stmt(pass_stmt *obj) {
  // Nothing to do
}
void type_checker::visit_print_stmt(print_stmt *obj) {
  obj->expression_->accept(this);
  pop();
}
void type_checker::visit_return_stmt(return_stmt *obj) {
  auto function_name = peek_function();
  obj->expression_->accept(this);
  auto return_data_type = pop();
  if (function_name.empty() || !this->functions_.has(function_name)) {
    error(obj->return_keyword_, "Invalid use of return statement");
  } else {
    // func cannot be null here.
    auto func = this->functions_.get(function_name);
    if (convert_data_type(func->return_type_->name_) !=
        return_data_type.object_type_) {
      error(obj->return_keyword_, "Invalid return data type");
    }
  }
}
void type_checker::visit_while_stmt(while_stmt *obj) {
  obj->expression_->accept(this);
  auto exp = pop();
  if (exp.object_type_ != object_type::BOOL) {
    error(obj->while_keyword_,
          "While statement expression need to be a boolean");
  }
  push_scope_type(ast_type::STMT_WHILE);
  scope_.push();
  obj->while_body_->accept(this);
  scope_.pop();
  pop_scope_type();
}
void type_checker::check(const std::vector<stmt *> &statements) {
  functions_.extract(statements);
  for (const auto &err : functions_.errors_) { errors_.emplace_back(err); }
  auto main_function_name = prefix("main");
  if (!functions_.has(main_function_name)) {
    error("Critical !! main() function must be present");
  } else {
    auto main_function = functions_.get(main_function_name);
    if (!main_function->params_.empty()) {
      error("Critical !! main() function must not have parameters");
    }
    if (convert_data_type(main_function->return_type_->name_) !=
        object_type::INTEGER) {
      error("Critical !! main() function must return an integer");
    }
  }
  for (const auto &name : functions_.function_names_) {
    auto function_definition = functions_.get(name);
    if (function_definition->params_.size() > 100) {
      error(function_definition->name_,
            "Number of parameters cannot be larger than 100.");
    }
    auto function_placeholder_object = ykobject();
    function_placeholder_object.object_type_ = object_type::FUNCTION;
    scope_.define_global(name, function_placeholder_object);
  }
}
void type_checker::error(token *tok, const std::string &message) {
  auto err = parsing_error{message, tok};
  errors_.emplace_back(err);
}
void type_checker::error(const std::string &message) {
  auto err = parsing_error{message, "", 0, 0};
  err.token_set_ = false;
  errors_.emplace_back(err);
}
void type_checker::push(const ykobject &data_type) {
  this->object_stack_.push_back(data_type);
}
ykobject type_checker::pop() {
  if (this->object_stack_.empty()) { return ykobject(); }
  auto back = object_stack_.back();
  object_stack_.pop_back();
  return back;
}
bool type_checker::match_data_type(token *type_in_code,
                                   const ykobject &type_in_checker) {
  return convert_data_type(type_in_code) == type_in_checker.object_type_;
}
object_type type_checker::convert_data_type(token *basic_dt) {
  auto dt = basic_dt->token_;
  if (dt == "str") {
    return object_type::STRING;
  } else if (dt == "int" || dt == "i32") {
    return object_type::INTEGER;
  } else if (dt == "float") {
    return object_type::DOUBLE;
  }
  return object_type::NONE_OBJ;
}
void type_checker::push_scope_type(ast_type scope_type) {
  this->scope_type_stack_.emplace_back(scope_type);
}
ast_type type_checker::peek_scope_type() {
  if (this->scope_type_stack_.empty()) {
    return ast_type::STMT_PASS;// Pass is used for unknown
  }
  return this->scope_type_stack_.back();
}
void type_checker::pop_scope_type() {
  if (this->scope_type_stack_.empty()) { return; }
  this->scope_type_stack_.pop_back();
}
void type_checker::push_function(const std::string &prefixed_name) {
  this->function_name_stack_.emplace_back(prefixed_name);
}
std::string type_checker::peek_function() {
  if (this->function_name_stack_.empty()) { return ""; }
  return this->function_name_stack_.back();
}
void type_checker::pop_function() {
  if (this->function_name_stack_.empty()) { return; }
  this->function_name_stack_.pop_back();
}
void type_checker::visit_defer_stmt(defer_stmt *obj) {
  auto st = expression_stmt{obj->expression_};
  this->visit_expression_stmt(&st);
}
