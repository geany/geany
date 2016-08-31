"""
Test arglist parsing of a class' __init__ method:
the __init__() method's argument list should be assigned to the class' argument list.

This is somewhat special to Python and the Python parses uses tm_source_file_set_tag_arglist()
to do this and tm_source_file_set_tag_arglist() uses tm_tags_find() which by default relies on
a sorted tags array. However, when the parses uses tm_source_file_set_tag_arglist() the tags
array is *not yet* sorted and so it might be the tag for the class SomeClass can't be found,
hence this test.
"""

from bsddb import btopen
import sys
from django.contrib.auth.decorators import login_required, permission_required
from django.http import HttpResponse, HttpResponseBadRequest
from django.shortcuts import render_to_response
from django.template.context import RequestContext
from django.utils import simplejson
from django.views.decorators.csrf import csrf_exempt
from django.views.decorators.csrf import csrf_exempt2
from django.views.decorators.http import require_POST
from vnstat.api.error import InterfaceDataValidationError


class SomeClass(object):

    def __init__(self, filename, pathsep='', treegap=64):
        pass
