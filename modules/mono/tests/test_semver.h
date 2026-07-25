/**************************************************************************/
/*  test_semver.h                                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once

#include "tests/test_macros.h"

#include "../editor/semver.h"

// A4: SemVer 单元测试。覆盖 SemVer 2.0.0 规范的核心场景：
//   - 解析：基本版本 / 预发布 / 构建元数据 / 完整版本 / dotnet 输出 / 非法输入
//   - 比较：等价 / major/minor/patch 优先级 / 预发布优先级（数值字段、字母字段、字段数）
// 参考资料：
//   - https://semver.org/spec/v2.0.0.html
//   - dotnet --version 输出格式（如 "8.0.100"）
// 运行：godot --test --test-case=*SemVer*
//
// 实现说明：遵循 Godot 模块测试标准模式（参考 modules/zip/tests/test_zip.h），
// TEST_CASE 宏直接在头文件中定义，由 modules_tests.gen.h 包含到 test_main.cpp，
// 确保 doctest 能正确注册测试用例。

namespace TestSemVer {

using godotsharp::SemVer;
using godotsharp::SemVerParser;

// ============================================================================
// Parsing tests
// ============================================================================

TEST_CASE("[SemVer] Parse basic version") {
	SemVerParser parser;
	SemVer v;
	CHECK(parser.parse("1.2.3", v));
	CHECK(v.major == 1);
	CHECK(v.minor == 2);
	CHECK(v.patch == 3);
	CHECK(v.prerelease.is_empty());
	CHECK(v.build_metadata.is_empty());
}

TEST_CASE("[SemVer] Parse version with prerelease") {
	SemVerParser parser;
	SemVer v;
	CHECK(parser.parse("1.2.3-beta.1", v));
	CHECK(v.major == 1);
	CHECK(v.minor == 2);
	CHECK(v.patch == 3);
	CHECK(v.prerelease == "beta.1");
	CHECK(v.build_metadata.is_empty());
}

TEST_CASE("[SemVer] Parse version with build metadata") {
	// 构建元数据不影响版本优先级，但必须被正确解析
	SemVerParser parser;
	SemVer v;
	CHECK(parser.parse("1.2.3+build.2026", v));
	CHECK(v.major == 1);
	CHECK(v.minor == 2);
	CHECK(v.patch == 3);
	CHECK(v.prerelease.is_empty());
	CHECK(v.build_metadata == "build.2026");
}

TEST_CASE("[SemVer] Parse full version with prerelease and build metadata") {
	SemVerParser parser;
	SemVer v;
	CHECK(parser.parse("1.2.3-beta.1+build.2026", v));
	CHECK(v.major == 1);
	CHECK(v.minor == 2);
	CHECK(v.patch == 3);
	CHECK(v.prerelease == "beta.1");
	CHECK(v.build_metadata == "build.2026");
}

TEST_CASE("[SemVer] Parse dotnet version output") {
	// P4 构建面板会用 SemVerParser 解析 dotnet --version 输出以验证 SDK 版本
	SemVerParser parser;
	SemVer v;
	CHECK(parser.parse("8.0.100", v));
	CHECK(v.major == 8);
	CHECK(v.minor == 0);
	CHECK(v.patch == 100);
	CHECK(v.prerelease.is_empty());
	CHECK(v.build_metadata.is_empty());

	// dotnet SDK 预览版格式
	CHECK(parser.parse("9.0.100-preview.1.24081.5", v));
	CHECK(v.major == 9);
	CHECK(v.minor == 0);
	CHECK(v.patch == 100);
	CHECK(v.prerelease == "preview.1.24081.5");
}

TEST_CASE("[SemVer] Reject invalid version strings") {
	SemVerParser parser;
	SemVer v;

	// 缺少 patch 字段
	CHECK_FALSE(parser.parse("1.2", v));

	// 多余字段
	CHECK_FALSE(parser.parse("1.2.3.4", v));

	// 前导零（SemVer 规范禁止）
	CHECK_FALSE(parser.parse("01.2.3", v));
	CHECK_FALSE(parser.parse("1.02.3", v));
	CHECK_FALSE(parser.parse("1.2.03", v));

	// 非SemVer前缀
	CHECK_FALSE(parser.parse("v1.2.3", v));

	// 空字符串
	CHECK_FALSE(parser.parse("", v));

	// 字母字段（非预发布/构建元数据）
	CHECK_FALSE(parser.parse("a.b.c", v));

	// 负数
	CHECK_FALSE(parser.parse("-1.2.3", v));
}

// ============================================================================
// Comparison tests
// ============================================================================

TEST_CASE("[SemVer] Compare version equality") {
	SemVerParser parser;
	SemVer a, b;

	CHECK(parser.parse("1.2.3", a));
	CHECK(parser.parse("1.2.3", b));
	CHECK(a == b);
	CHECK_FALSE(a != b);
	CHECK_FALSE(a < b);
	CHECK_FALSE(a > b);
	CHECK(a <= b);
	CHECK(a >= b);

	// 构建元数据不影响等价性（按 SemVer 规范，构建元数据被忽略）
	CHECK(parser.parse("1.2.3+build.1", a));
	CHECK(parser.parse("1.2.3+build.2", b));
	CHECK(a == b);
}

TEST_CASE("[SemVer] Compare major version precedence") {
	SemVerParser parser;
	SemVer a, b;

	CHECK(parser.parse("1.0.0", a));
	CHECK(parser.parse("2.0.0", b));
	CHECK(a < b);
	CHECK(b > a);
	CHECK(a != b);
	CHECK_FALSE(a == b);
}

TEST_CASE("[SemVer] Compare minor version precedence") {
	SemVerParser parser;
	SemVer a, b;

	CHECK(parser.parse("1.0.0", a));
	CHECK(parser.parse("1.1.0", b));
	CHECK(a < b);
	CHECK(b > a);

	CHECK(parser.parse("1.9.0", a));
	CHECK(parser.parse("1.10.0", b));
	CHECK(a < b);
}

TEST_CASE("[SemVer] Compare patch version precedence") {
	SemVerParser parser;
	SemVer a, b;

	CHECK(parser.parse("1.0.0", a));
	CHECK(parser.parse("1.0.1", b));
	CHECK(a < b);
	CHECK(b > a);

	CHECK(parser.parse("1.0.9", a));
	CHECK(parser.parse("1.0.10", b));
	CHECK(a < b);
}

TEST_CASE("[SemVer] Compare prerelease vs release precedence") {
	// SemVer 规范：1.0.0-alpha < 1.0.0
	SemVerParser parser;
	SemVer a, b;

	CHECK(parser.parse("1.0.0-alpha", a));
	CHECK(parser.parse("1.0.0", b));
	CHECK(a < b);
	CHECK(b > a);
	CHECK(a != b);
}

TEST_CASE("[SemVer] Compare prerelease numeric fields") {
	// 纯数字字段按数值比较：1.0.0-alpha.1 < 1.0.0-alpha.2 < ... < 1.0.0-alpha.10
	SemVerParser parser;
	SemVer a, b;

	CHECK(parser.parse("1.0.0-alpha.1", a));
	CHECK(parser.parse("1.0.0-alpha.2", b));
	CHECK(a < b);

	CHECK(parser.parse("1.0.0-alpha.9", a));
	CHECK(parser.parse("1.0.0-alpha.10", b));
	CHECK(a < b);

	CHECK(parser.parse("1.0.0-alpha.99", a));
	CHECK(parser.parse("1.0.0-alpha.100", b));
	CHECK(a < b);
}

TEST_CASE("[SemVer] Compare prerelease alphanumeric fields") {
	// 数字标识符优先级低于字母标识符：1.0.0-alpha.1 < 1.0.0-alpha.beta
	SemVerParser parser;
	SemVer a, b;

	CHECK(parser.parse("1.0.0-alpha.1", a));
	CHECK(parser.parse("1.0.0-alpha.beta", b));
	CHECK(a < b);
	CHECK(b > a);

	// 字典序比较
	CHECK(parser.parse("1.0.0-alpha", a));
	CHECK(parser.parse("1.0.0-beta", b));
	CHECK(a < b);

	CHECK(parser.parse("1.0.0-rc.1", a));
	CHECK(parser.parse("1.0.0-alpha.1", b));
	CHECK(a > b); // rc > alpha
}

TEST_CASE("[SemVer] Compare prerelease field count precedence") {
	// 字段数多者优先（当前缀字段全部相等时）：1.0.0-alpha < 1.0.0-alpha.1
	SemVerParser parser;
	SemVer a, b;

	CHECK(parser.parse("1.0.0-alpha", a));
	CHECK(parser.parse("1.0.0-alpha.1", b));
	CHECK(a < b);
	CHECK(b > a);
}

} // namespace TestSemVer
