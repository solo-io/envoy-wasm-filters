Transform headers using inja templates!

Example Config:

```json
{
    "transformations": {
        "match": {
            "header_matchers": [
                {
                    "name": ":path",
                    "prefix_match": "/foo"
                }
            ]
        },
        "route_transformations": {
            "response_transformation": {
                "headers": {
                    "x-yuval": {
                        "text": "{{header(\":status\")}}"
                    },
                    "x-yuval2": {
                        "text": "foo"
                    }
                }
            }
        }
    }
}
```