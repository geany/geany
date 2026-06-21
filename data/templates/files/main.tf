{fileheader}
terraform {
  required_providers {
      other = {
      source = "dev/other"
      version = ""
    }
  }
}

provider "other" {
}

resource "my_resource" "other_res" {
}

output "something" {
  value = ""
}
