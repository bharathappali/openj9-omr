/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2014, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "EventTypes.hpp"
#include "FileUtils.hpp"
#include "TraceHeaderWriter.hpp"


const char *UT_H_FILE_FOOTER_TEMPLATE =
"extern UtModuleInfo %s_UtModuleInfo;\n"
"extern unsigned char %s_UtActive[];\n"
"\n"
"#ifndef UT_MODULE_INFO\n"
"#define UT_MODULE_INFO %s_UtModuleInfo\n"
"#endif /* UT_MODULE_INFO */\n"
"\n"
"#ifndef UT_ACTIVE\n"
"#define UT_ACTIVE %s_UtActive\n"
"#endif /* UT_ACTIVE */\n"
"\n"
"#ifdef __cplusplus\n"
"} /* extern \"C\" */\n"
"#endif\n"
"#endif /* UTE_%s_MODULE_HEADER */\n"
"/*\n"
" * End of file\n"
" */\n";

const char *UT_H_FILE_HEADER_TEMPLATE =
"/*\n"
" *  Do not edit this file \n"
" *  Generated by TraceGen\n"
" */\n"
"#ifndef UTE_%s_MODULE_HEADER\n"
"#define UTE_%s_MODULE_HEADER\n"
"#include \"ute_module.h\"\n"
"#if !defined(UT_DIRECT_TRACE_REGISTRATION)\n"
"#include \"jni.h\"\n"
"#endif /* !defined(UT_DIRECT_TRACE_REGISTRATION) */\n"
"#ifndef UT_TRACE_OVERHEAD\n"
"#define UT_TRACE_OVERHEAD %u\n"
"#endif\n"
"#ifndef UT_THREAD\n"
"#define UT_THREAD(thr) (void *)thr\n"
"#endif /* UT_THREAD */\n"
"#ifndef UT_STR\n"
"#define UT_STR(arg) #arg\n"
"#endif\n"
"#ifdef __cplusplus\n"
"extern \"C\" {\n"
"#endif\n"
"\n"
"#ifdef __clang__\n"
"#include <unistd.h>\n"
"#define Trace_Unreachable() _exit(-1)\n"
"#else\n"
"#define Trace_Unreachable()\n"
"#endif\n"
"\n"
"#if defined(UT_DIRECT_TRACE_REGISTRATION)\n"
"int32_t register%sWithTrace(UtInterface * utIntf, UtModuleInfo* containerName);\n"
"int32_t deregister%sWithTrace(UtInterface * utIntf);\n"
"#define UT_MODULE_LOADED(utIntf) register%sWithTrace((utIntf), NULL);\n"
"#define UT_MODULE_UNLOADED(utIntf) deregister%sWithTrace((utIntf));\n"
"#define UT_%s_MODULE_LOADED(utIntf) register%sWithTrace((utIntf), NULL);\n"
"#define UT_%s_MODULE_UNLOADED(utIntf) deregister%sWithTrace((utIntf));\n"
"#else /* defined(UT_DIRECT_TRACE_REGISTRATION) */\n"
"int32_t register%sWithTrace(JavaVM * vm, UtModuleInfo* containerName);\n"
"int32_t deregister%sWithTrace(JavaVM * vm);\n"
"#define UT_MODULE_LOADED(vm) register%sWithTrace((JavaVM *)(vm), NULL);\n"
"#define UT_MODULE_UNLOADED(vm) deregister%sWithTrace((JavaVM *)(vm));\n"
"#define UT_%s_MODULE_LOADED(vm) register%sWithTrace((JavaVM *)(vm), NULL);\n"
"#define UT_%s_MODULE_UNLOADED(vm) deregister%sWithTrace((JavaVM *)(vm));\n"
"#endif /* defined(UT_DIRECT_TRACE_REGISTRATION) */\n"
"\n";

const char *TP_ASSERT_TEMPLATE =
"#if UT_TRACE_OVERHEAD >= %u\n"
"%s" /* Place holder for option test macro (specified by "Test" option in tp spec) */
"#define %s(%s) do { /* tracepoint name: %s.%u */ \\\n"
"	if ((unsigned char) %s_UtActive[%u] != 0){ \\\n"
"		if (%s) { /* assertion satisfied */ } else { \\\n"
"			if (%s_UtModuleInfo.intf != NULL) { \\\n"
"				%s_UtModuleInfo.intf->Trace(%s, &%s_UtModuleInfo, (UT_SPECIAL_ASSERTION | (%uu << 8) | %s_UtActive[%u]), \"\\377\\4\\377\", __FILE__, __LINE__, UT_STR((%s))); \\\n"
"				Trace_Unreachable(); \\\n"
"			} else { \\\n"
"				fprintf(stderr, \"** ASSERTION FAILED ** %s.%u at %%s:%%d %s%%s\\n\", __FILE__, __LINE__, UT_STR((%s))); \\\n"
"			} \\\n"
"		}} \\\n"
"	} while(0)\n"
"#else\n"
"%s" /* Place holder for option test macro (specified by "Test" option in tp spec) */
"#define %s(%s)   /* tracepoint name: %s.%u */\n"
"#endif\n\n";

const char *TP_AUX_TEMPLATE =
"#if UT_TRACE_OVERHEAD >= %u\n"
"%s" /* Place holder for option test macro (specified by "Test" option in tp spec) */
"#define %s(%s%s) do { /* tracepoint name: %s.%u */ \\\n"
"		%s_UtModuleInfo.intf->Trace(%s, &%s_UtModuleInfo, ((%uu << 8)), %s%s);	} while(0)\n"
"#else\n"
"%s" /* Place holder for option test macro (specified by "Test" option in tp spec) */
"#define %s(%s%s)   /* tracepoint name: %s.%u */\n"
"#endif\n\n";

const char *TP_TEMPLATE =
"#if UT_TRACE_OVERHEAD >= %u\n"
"%s" /* Place holder for option test macro (specified by "Test" option in tp spec) */
"#define %s(%s%s) do { /* tracepoint name: %s.%u */ \\\n"
"	if ((unsigned char) %s_UtActive[%u] != 0){ \\\n"
"		%s_UtModuleInfo.intf->Trace(%s, &%s_UtModuleInfo, ((%uu << 8) | %s_UtActive[%u]), %s%s);} \\\n"
"	} while(0)\n"
"#else\n"
"%s" /* Place holder for option test macro (specified by "Test" option in tp spec) */
"#define %s(%s%s)   /* tracepoint name: %s.%u */\n"
"#endif\n\n";

RCType
TraceHeaderWriter::writeOutputFiles(J9TDFOptions *options, J9TDFFile *tdf)
{
	RCType rc = RC_OK;
	unsigned int id = 0;
	J9TDFTracepoint *tp = NULL;
	FILE *fd = NULL;

	const char *fileName = FileUtils::getTargetFileName(options, tdf->fileName, UT_FILENAME_PREFIX, tdf->header.executable, ".h");

	time_t sourceFileMtime = FileUtils::getMtime(tdf->fileName);
	time_t targetFileMtime = FileUtils::getMtime(fileName);

	if (false == options->force && targetFileMtime > sourceFileMtime) {
		printf("Header file is already up-to-date: %s\n", fileName);
		Port::omrmem_free((void **)&fileName);
		return RC_OK;
	}

	printf("Creating header file: %s\n", fileName);

	fd = Port::fopen(fileName, "wb");

	if (NULL == fd) {
		perror("Error opening file");
		goto failed;
	}

	headerTemplate(options, fd, tdf->header.executable);

	tp = tdf->tracepoints;
	while (NULL != tp) {
		if (!tp->obsolete) {
			if (UT_ASSERT_TYPE == tp->type) {
				tpAssert(fd, tp->overhead, tp->test, tp->name, tdf->header.executable, id, tp->hasEnv, tp->format, tp->parmCount);
			} else {
				tpTemplate(fd, tp->overhead, tp->test, tp->name, tdf->header.executable, id, tp->hasEnv, tp->parameters, tp->parmCount, tdf->header.auxiliary);
			}
		}
		id++;
		tp = tp->nexttp;
	}
	/* Write the footer. */
	footerTemplate(fd, tdf->header.executable);
	if (0 == fclose(fd)) {
		rc = RC_OK;
	} else {
		eprintf("Failed to close file %s", fileName);
		goto failed;
	}

	Port::omrmem_free((void **)&fileName);
	return rc;

failed:
	Port::omrmem_free((void **)&fileName);
	return RC_FAILED;
}

/* Standard trace point template.
 * Parameters to printf should be:
 * 1 - overhead (int)
 * 2 - trace point name (char *)
 * 3 - module name (char *)
 * 4 - trace point id (int)
 * 5 - a string containing "UT_THREAD(thr)" or "(void *)NULL" if NoEnv was specified for this trace point.
 * 6 - NULL or trace point argument string and varargs parameters.
 *
 * Note: windows doesn't support doing %2$s (which is a bit of a pain)
 */
RCType
TraceHeaderWriter::tpTemplate(FILE *fd, unsigned int overhead, unsigned int test, const char *name, const char *module, unsigned int id, unsigned int envParam, const char *parameters, unsigned int parmCount, unsigned int auxiliary)
{
	RCType rc = RC_FAILED;

	/* 5 characters allows "P999, " or nearly 1000 parameters. */
	char *parmString = NULL;
	char *parmStringNoLeadingComma = NULL;
	char *pos = NULL;
	char *testMacro =  NULL;
	char *testNop =  NULL;
	char *testMacroTemplate = (char *)  "#define TrcEnabled_%s  (%s_UtActive[%u] != 0)\n";
	char *testNopTemplate = (char *) "#define TrcEnabled_%s  (0)\n";

	parmString = (char *)Port::omrmem_calloc(1, (parmCount * sizeof(char) * 5) + 1);
	if (NULL == parmString) {
		eprintf("Failed to allocate memory");
		goto failed;
	}

	if (parmCount > 0) {
		parmStringNoLeadingComma = parmString + 2;
	} else {
		parmStringNoLeadingComma = parmString;
	}
	pos = parmString;

	if (test) {
		/* Allow 7 digits for tracepoints + 1 for the null byte. (Millions of trace points are unlikely.) */
		testMacro = (char *)Port::omrmem_calloc(1, (strlen(testMacroTemplate) + strlen(name) + strlen(module) + 8));
		if (NULL == testMacro) {
			eprintf("Failed to allocate memory");
			goto failed;
		}
		sprintf(testMacro, testMacroTemplate, name, module, id);

		testNop = (char *)Port::omrmem_calloc(1, (strlen(testMacroTemplate) + strlen(name) + 1));
		if (NULL == testNop) {
			eprintf("Failed to allocate memory");
			goto failed;
		}
		sprintf(testNop, testNopTemplate, name);
	} else {
		testMacro = (char *) "";
		testNop = (char *) "";
	}

	for (unsigned int i = 0; i < parmCount; i++) {
		pos += sprintf(pos, ", P%u", i + 1);
	}

	if (auxiliary) {
		if (0 < fprintf(fd, TP_AUX_TEMPLATE
				, overhead
				, testMacro
				, name
				, envParam ? "thr" : ""
				, envParam ? parmString : parmStringNoLeadingComma
				, module
				, id
				, module
				, envParam ? UT_ENV_PARAM : UT_NOENV_PARAM
				, module
				, id
				, parameters
				, parmString
				, testNop
				, name
				, envParam ? "thr" : ""
				, envParam ? parmString : parmStringNoLeadingComma
				, module
				, id
		)) {
			rc = RC_OK;
		} else {
			rc = RC_FAILED;
			goto failed;
		}
	} else {
		if (0 <= fprintf(fd, TP_TEMPLATE
				, overhead
				, testMacro
				, name
				, envParam ? "thr" : ""
				, envParam ? parmString : parmStringNoLeadingComma
				, module
				, id
				, module
				, id
				, module
				, envParam ? UT_ENV_PARAM : UT_NOENV_PARAM
				, module
				, id
				, module
				, id
				, parameters
				, parmString
				, testNop
				, name
				, envParam ? "thr" : ""
				, envParam ? parmString : parmStringNoLeadingComma
				, module
				, id
		)) {
			rc = RC_OK;
		} else {
			rc = RC_FAILED;
			goto failed;
		}
	}

	Port::omrmem_free((void **)&parmString);

	if (test) {
		Port::omrmem_free((void **)&testMacro);
		Port::omrmem_free((void **)&testNop);
	}

	return rc;

failed:
	Port::omrmem_free((void **)&parmString);

	if (test) {
		Port::omrmem_free((void **)&testMacro);
		Port::omrmem_free((void **)&testNop);
	}

	return rc;
}

RCType
TraceHeaderWriter::tpAssert(FILE *fd, unsigned int overhead, unsigned int test, const char *name, const char *module, unsigned int id, unsigned int envParam, const char *conditionStr, unsigned int parmCount)
{
	/* 5 characters allows "P999, " or nearly 1000 parameters. */
	RCType rc = RC_FAILED;
	char *parmString = NULL;
	char *parmStringNoLeadingComma = NULL;
	char *pos = NULL;
	char *testmacro = NULL;
	char *testnop = NULL;
	char *testmacrotemplate = (char *)  "#define TrcEnabled_%s  (%s_UtActive[%u] != 0)\n";
	char *testnoptemplate = (char *) "#define TrcEnabled_%s  (0)\n";

	parmString = (char *)Port::omrmem_calloc(1, (parmCount * sizeof(char) * 5) + 1);
	if (NULL == parmString) {
		eprintf("Failed to allocate memory");
		goto failed;
	}

	if (parmCount > 0) {
		parmStringNoLeadingComma = parmString + 2;
	} else {
		parmStringNoLeadingComma = parmString;
	}

	pos = parmString;

	if (test) {
		/* Allow 7 digits for tracepoints + 1 for the null byte. (Millions of trace points are unlikely.) */
		testmacro = (char *)Port::omrmem_calloc(1, (strlen(testmacrotemplate) + strlen(name) + strlen(module) + 8));
		if (NULL == testmacro) {
			eprintf("Failed to allocate memory");
			goto failed;
		}
		sprintf(testmacro, testmacrotemplate, name, module, id);

		testnop = (char *)Port::omrmem_calloc(1, (strlen(testmacrotemplate) + strlen(name) + 1));
		if (NULL == testnop) {
			eprintf("Failed to allocate memory");
			goto failed;
		}
		sprintf(testnop, testnoptemplate, name);
	} else {
		testmacro = (char *) "";
		testnop = (char *) "";
	}

	for (unsigned int i = 0; i < parmCount; i++) {
		pos += sprintf(pos, ", P%u", i + 1);
	}

	if (0 <= fprintf(fd, TP_ASSERT_TEMPLATE
			, overhead
			, testmacro
			, name, parmStringNoLeadingComma
			, module
			, id
			, module
			, id
			, conditionStr
			, module
			, module
			, envParam ? "thr" : "(void *)NULL"
			, module
			, id
			, module
			, id
			, conditionStr
			, module
			, id
			, name
			, conditionStr
			, testnop
			, name
			, parmStringNoLeadingComma
			, module
			, id)) {
		rc = RC_OK;
	} else {
		rc = RC_FAILED;
		goto failed;
	}

	if (test) {
		Port::omrmem_free((void **)&testmacro);
		Port::omrmem_free((void **)&testnop);
	}

	Port::omrmem_free((void **)&parmString);
	return rc;

failed:

	if (test) {
		Port::omrmem_free((void **)&testmacro);
		Port::omrmem_free((void **)&testnop);
	}

	Port::omrmem_free((void **)&parmString);

	return rc;
}

RCType
TraceHeaderWriter::headerTemplate(J9TDFOptions *options, FILE *fd, const char *moduleName)
{
	RCType rc = RC_FAILED;
	char *ucModule = NULL;
	char *pos = NULL;

	ucModule = (char *)Port::omrmem_calloc(1, strlen(moduleName) + 1);
	if (NULL == ucModule) {
		eprintf("Failed to allocate memory");
		goto failed;
	}
	pos = ucModule;

	strcpy(ucModule, moduleName);
	while ('\0' != *pos) {
		*pos = toupper(*pos);
		pos++;
	}
	if (0 <= fprintf(fd, UT_H_FILE_HEADER_TEMPLATE,
			ucModule,
			ucModule,
			options->threshold,
			moduleName,
			moduleName,
			moduleName,
			moduleName,
			ucModule, moduleName,
			ucModule, moduleName,
			moduleName,
			moduleName,
			moduleName,
			moduleName,
			ucModule, moduleName,
			ucModule, moduleName
			)) {
		rc = RC_OK;
	} else {
		rc = RC_FAILED;
		goto failed;
	}

	Port::omrmem_free((void **)&ucModule);
	return rc;

failed:
	Port::omrmem_free((void **)&ucModule);

	return rc;
}

RCType
TraceHeaderWriter::footerTemplate(FILE *fd, const char *moduleName)
{
	char *ucModule = NULL;
	char *pos = NULL;
	RCType rc = RC_FAILED;

	ucModule = (char *)Port::omrmem_calloc(1, strlen(moduleName) + 1);
	if (NULL == ucModule) {
		eprintf("Failed to allocate memory");
		goto failed;
	}
	pos = ucModule;

	strcpy(ucModule, moduleName);
	while ('\0' != *pos) {
		*pos = toupper(*pos);
		pos++;
	}
	if (0 <= fprintf(fd, UT_H_FILE_FOOTER_TEMPLATE, moduleName, moduleName, moduleName, moduleName, ucModule)) {
		rc = RC_OK;
	} else {
		rc = RC_FAILED;
		goto failed;
	}

	Port::omrmem_free((void **)&ucModule);
	return rc;

failed:
	Port::omrmem_free((void **)&ucModule);

	return rc;
}
